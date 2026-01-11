// src/processYear.zig
const std = @import("std");
const helper = @import("helper.zig");
const schema = @import("schemaStructs.zig");

const fifoSnapshot = @import("fifoSnapshot.zig");
const fifoRealized = @import("fifoRealized.zig");
const readTrades = @import("readTrades.zig");
const meta = @import("sqliteMeta.zig");

const DbHandle = helper.DbHandle;

// -------------------------------
// BUY (broker,ticker) index
// -------------------------------

const PairKey = struct {
    broker: schema.zp_broker_buf, // [64]u8
    ticker: schema.zp_ticker, // [32]u8

    pub fn fromTrade(t: *const schema.zp_trade) PairKey {
        return .{ .broker = t.broker, .ticker = t.ticker };
    }
};

const PairKeyCtx = struct {
    pub fn hash(_: @This(), k: PairKey) u64 {
        var h = std.hash.Wyhash.init(0);
        h.update(k.broker[0..]);
        h.update(k.ticker[0..]);
        return h.final();
    }

    pub fn eql(_: @This(), a: PairKey, b: PairKey) bool {
        return std.mem.eql(u8, a.broker[0..], b.broker[0..]) and
            std.mem.eql(u8, a.ticker[0..], b.ticker[0..]);
    }
};

const BuyIndexList = std.ArrayListUnmanaged(usize);
const BuyIndexMap = std.HashMapUnmanaged(PairKey, BuyIndexList, PairKeyCtx, 80);

fn cstr0_from_buf(buf: []const u8) [*:0]const u8 {
    // Your ABI buffers are NUL-terminated by your DB readers / writers.
    return @as([*:0]const u8, @ptrCast(buf.ptr));
}

pub fn sqlite_process_year_trades_only(handle: DbHandle, year: u32) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (year == 0) return .invalid_argument;

    const allocator = std.heap.c_allocator;

    // ------------------------------------------------------------
    // 1) Load current year BUY trades (local buffer only)
    // ------------------------------------------------------------
    var buy_rows: usize = 0;
    var ec = meta.sqlite_count_buy_trades_by_year(handle, year, &buy_rows);
    if (ec != .ok) return ec;

    var buy_buf: ?[]schema.zp_trade = null;
    defer if (buy_buf) |b| allocator.free(b);

    if (buy_rows > 0) {
        var tmp = allocator.alloc(schema.zp_trade, buy_rows) catch return .preparation_fail;
        errdefer allocator.free(tmp);

        var got: usize = 0;
        ec = readTrades.sqlite_read_buy_trades_by_year(handle, year, tmp.ptr, tmp.len, &got);
        if (ec != .ok) return ec;

        buy_buf = tmp[0..got];
    }

    // ------------------------------------------------------------
    // 1b) Convert BUY trades into snapshot rows + build (broker,ticker)->indices map
    // ------------------------------------------------------------
    var buy_snap_buf: ?[]schema.zp_fifo_snapshot = null;
    defer if (buy_snap_buf) |b| allocator.free(b);

    var buy_map: BuyIndexMap = .{};
    defer {
        var it = buy_map.iterator();
        while (it.next()) |e| {
            e.value_ptr.*.deinit(allocator);
        }
        buy_map.deinit(allocator);
    }

    if (buy_buf) |buys| {
        var tmp = allocator.alloc(schema.zp_fifo_snapshot, buys.len) catch return .preparation_fail;
        errdefer allocator.free(tmp);

        const cap_u32: u32 = std.math.cast(u32, buys.len) orelse return .preparation_fail;
        buy_map.ensureTotalCapacity(allocator, cap_u32) catch return .preparation_fail;

        var j: usize = 0;
        while (j < buys.len) : (j += 1) {
            const row = buys[j];

            var snap: schema.zp_fifo_snapshot = schema.zp_fifo_snapshot.zero();
            snap.tax_year = year;

            snap.broker = row.broker;
            snap.ticker = row.ticker;
            snap.acq_trade_id = row.id;
            snap.acq_datetime = row.trade_datetime;

            snap.qty_remaining = row.quantity;
            snap.cost_per_share_eur = row.price_per_share * row.conversion_rate_eur;
            snap.acq_commission_eur = row.commission * row.conversion_rate_eur;
            snap.country = row.country;

            tmp[j] = snap;

            // Map (broker,ticker) -> [indices into buy_snap_buf]
            const key = PairKey.fromTrade(&row);
            const gop = buy_map.getOrPut(allocator, key) catch return .preparation_fail;
            if (!gop.found_existing) gop.value_ptr.* = .{};
            gop.value_ptr.*.append(allocator, j) catch return .preparation_fail;
        }

        buy_snap_buf = tmp;
    }

    // ------------------------------------------------------------
    // 2) Count current year SELL trades
    // ------------------------------------------------------------
    var sell_rows: usize = 0;
    ec = meta.sqlite_count_sell_trades_by_year(handle, year, &sell_rows);
    if (ec != .ok) return ec;

    // ------------------------------------------------------------
    // If there are NO sells: insert all BUY snapshots and return OK.
    // ------------------------------------------------------------
    if (sell_rows == 0) {
        if (buy_snap_buf) |snaps| {
            var k: usize = 0;
            while (k < snaps.len) : (k += 1) {
                if (!(snaps[k].qty_remaining > 0.0)) continue;

                ec = fifoSnapshot.sqlite_insert_fifo_snapshot(handle, &snaps[k]);
                if (ec != .ok) return ec;
            }
        }

        std.debug.print("Year {d}: BUY {d}, SELL 0\n", .{ year, buy_rows });
        return .ok;
    }

    // ------------------------------------------------------------
    // 3) Load current year SELL trades (local buffer only)
    // ------------------------------------------------------------
    var sell_buf: ?[]schema.zp_trade = null;
    defer if (sell_buf) |b| allocator.free(b);

    {
        var tmp = allocator.alloc(schema.zp_trade, sell_rows) catch return .preparation_fail;
        errdefer allocator.free(tmp);

        var got: usize = 0;
        ec = readTrades.sqlite_read_sell_trades_by_year(handle, year, tmp.ptr, tmp.len, &got);
        if (ec != .ok) return ec;

        sell_buf = tmp[0..got];
    }

    // ------------------------------------------------------------
    // 4) For each SELL:
    //    - consume DB open lots (fifo_snapshot up to year) FIFO-style
    //    - if still remaining, consume current-year BUY lots (buy_snap_buf) FIFO-style
    //    - if still remaining, error
    // ------------------------------------------------------------
    const sells = sell_buf orelse return .internal_error;

    var si: usize = 0;
    var realized_trade: schema.zp_fifo_realized = schema.zp_fifo_realized.zero();

    while (si < sells.len) : (si += 1) {
        const s = sells[si];

        var sell_remaining: f64 = s.quantity;
        if (!(sell_remaining > 0.0)) continue;

        // Base realized fields for this SELL
        realized_trade.broker = s.broker;
        realized_trade.tax_year = year;
        realized_trade.ticker = s.ticker;
        realized_trade.sell_trade_id = s.id;
        realized_trade.sell_datetime = s.trade_datetime;
        realized_trade.country = s.country;

        const broker_z: [*:0]const u8 = cstr0_from_buf(s.broker[0..]);
        const ticker_z: [*:0]const u8 = cstr0_from_buf(s.ticker[0..]);

        const broker_s: [:0]const u8 = std.mem.sliceTo(broker_z, 0);
        const ticker_s: [:0]const u8 = std.mem.sliceTo(ticker_z, 0);

        // match_seq must span DB-lots + current-year buys for this SELL
        var match_seq: u32 = 0;

        // -----------------------------
        // 4a) Consume DB open lots
        // -----------------------------
        var open_lot_rows: usize = 0;
        ec = meta.sqlite_count_rows(handle, .fifo_snapshot, year, broker_s, ticker_s, &open_lot_rows);
        if (ec != .ok) return ec;

        if (open_lot_rows > 0) {
            const lots = allocator.alloc(schema.zp_fifo_snapshot, open_lot_rows) catch return .preparation_fail;
            defer allocator.free(lots);

            var got_lots: usize = 0;
            ec = fifoSnapshot.sqlite_read_fifo_snapshot_by_year_broker_ticker(
                handle,
                year,
                broker_z,
                ticker_z,
                lots.ptr,
                lots.len,
                &got_lots,
            );
            if (ec != .ok) return ec;
            if (got_lots != open_lot_rows) return .read_row_fail;

            var li: usize = 0;
            while (li < got_lots and sell_remaining > 0.0) : (li += 1) {
                const lot = &lots[li];
                if (!(lot.qty_remaining > 0.0)) continue;

                const matched: f64 =
                    if (sell_remaining < lot.qty_remaining) sell_remaining else lot.qty_remaining;

                sell_remaining -= matched;

                const new_lot_remaining: f64 = lot.qty_remaining - matched;
                const lot_fully_depleted = (new_lot_remaining <= 0.0);
                const sell_fully_depleted = (sell_remaining <= 0.0);

                // DB lots: update DB
                if (lot_fully_depleted) {
                    ec = fifoSnapshot.sqlite_delete_fifo_snapshot_by_lot_id(handle, lot.lot_id);
                    if (ec != .ok) return ec;
                } else {
                    ec = fifoSnapshot.sqlite_update_fifo_snapshot_qty_remaining_by_lot_id(handle, lot.lot_id, new_lot_remaining);
                    if (ec != .ok) return ec;
                }

                realized_trade.buy_trade_id = lot.acq_trade_id;
                realized_trade.match_seq = match_seq;
                realized_trade.buy_datetime = lot.acq_datetime;
                realized_trade.qty_matched = matched;

                realized_trade.sale_value_eur = matched * s.price_per_share * s.conversion_rate_eur;
                realized_trade.acquisition_value_eur = matched * lot.cost_per_share_eur;

                realized_trade.costs_eur = 0.0;
                if (lot_fully_depleted) realized_trade.costs_eur += lot.acq_commission_eur;
                if (sell_fully_depleted) realized_trade.costs_eur += (s.commission * s.conversion_rate_eur);

                ec = fifoRealized.sqlite_insert_fifo_realized(handle, &realized_trade);
                if (ec != .ok) return ec;

                match_seq += 1;
            }
        }

        // -----------------------------
        // 4b) Consume current-year BUY lots (in-memory only)
        // -----------------------------
        if (sell_remaining > 0.0) {
            if (buy_snap_buf) |snaps| {
                const key = PairKey.fromTrade(&s);

                if (buy_map.get(key)) |idx_list| {
                    var bi: usize = 0;
                    while (bi < idx_list.items.len and sell_remaining > 0.0) : (bi += 1) {
                        const snap_idx = idx_list.items[bi];
                        if (snap_idx >= snaps.len) continue;

                        var lot = &snaps[snap_idx];
                        if (!(lot.qty_remaining > 0.0)) continue;

                        const matched: f64 =
                            if (sell_remaining < lot.qty_remaining) sell_remaining else lot.qty_remaining;

                        sell_remaining -= matched;

                        const new_lot_remaining: f64 = lot.qty_remaining - matched;
                        const lot_fully_depleted = (new_lot_remaining <= 0.0);
                        const sell_fully_depleted = (sell_remaining <= 0.0);

                        // In-memory lots: remove/update in buy_snap_buf so final snapshot insert skips depleted lots
                        if (lot_fully_depleted) {
                            lot.qty_remaining = 0.0;
                        } else {
                            lot.qty_remaining = new_lot_remaining;
                        }

                        realized_trade.buy_trade_id = lot.acq_trade_id;
                        realized_trade.match_seq = match_seq;
                        realized_trade.buy_datetime = lot.acq_datetime;
                        realized_trade.qty_matched = matched;

                        realized_trade.sale_value_eur = matched * s.price_per_share * s.conversion_rate_eur;
                        realized_trade.acquisition_value_eur = matched * lot.cost_per_share_eur;

                        realized_trade.costs_eur = 0.0;
                        if (lot_fully_depleted) realized_trade.costs_eur += lot.acq_commission_eur;
                        if (sell_fully_depleted) realized_trade.costs_eur += (s.commission * s.conversion_rate_eur);

                        ec = fifoRealized.sqlite_insert_fifo_realized(handle, &realized_trade);
                        if (ec != .ok) return ec;

                        match_seq += 1;
                    }
                }
            }
        }

        // -----------------------------
        // 4c) Hard fail if still unmatched
        // -----------------------------
        if (sell_remaining > 0.0) {
            std.debug.print(
                "ERROR: Unmatched SELL remaining qty. year={d} broker={s} ticker={s} sell_id={d} remaining={d}\n",
                .{
                    year,
                    broker_s,
                    ticker_s,
                    s.id,
                    sell_remaining,
                },
            );
            return .execution_fail;
        }
    }

    // ------------------------------------------------------------
    // 5) Insert remaining BUY snapshots (only those with qty_remaining > 0)
    // ------------------------------------------------------------
    if (buy_snap_buf) |snaps| {
        var k: usize = 0;
        while (k < snaps.len) : (k += 1) {
            if (!(snaps[k].qty_remaining > 0.0)) continue;

            ec = fifoSnapshot.sqlite_insert_fifo_snapshot(handle, &snaps[k]);
            if (ec != .ok) return ec;
        }
    }

    std.debug.print("Year {d}: BUY {d}, SELL {d}\n", .{ year, buy_rows, sell_rows });
    return .ok;
}
