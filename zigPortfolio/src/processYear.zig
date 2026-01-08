const std = @import("std");
const helper = @import("helper.zig");
const schema = @import("schemaStructs.zig");

const fifoSnapshot = @import("fifoSnapshot.zig");
const readTrades = @import("readTrades.zig");
const meta = @import("sqliteMeta.zig");

const DbHandle = helper.DbHandle;

fn has_year_buy_lots_for(
    buy_snaps: ?[]const schema.zp_fifo_snapshot,
    broker_z: [*:0]const u8,
    ticker_z: [*:0]const u8,
) bool {
    const snaps = buy_snaps orelse return false;

    // Compare as C-strings (safe because these arrays are NUL-terminated in your structs)
    const broker_s = std.mem.sliceTo(broker_z, 0);
    const ticker_s = std.mem.sliceTo(ticker_z, 0);

    var i: usize = 0;
    while (i < snaps.len) : (i += 1) {
        const b = std.mem.sliceTo(@as([*:0]const u8, @ptrCast(snaps[i].broker[0..].ptr)), 0);
        const t = std.mem.sliceTo(@as([*:0]const u8, @ptrCast(snaps[i].ticker[0..].ptr)), 0);

        if (std.mem.eql(u8, b, broker_s) and std.mem.eql(u8, t, ticker_s)) {
            // Also ensure there is positive remaining quantity (important once you start decrementing)
            if (snaps[i].qty_remaining > 0.0) return true;
        }
    }
    return false;
}

pub fn sqlite_process_year_load_only(handle: DbHandle, year: u32) helper.ErrorCode {
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
    // 1b) Convert BUY trades into snapshot rows (local buffer only)
    //     (Do this BEFORE checking sells, per your requirement.)
    // ------------------------------------------------------------
    var buy_snap_buf: ?[]schema.zp_fifo_snapshot = null;
    defer if (buy_snap_buf) |b| allocator.free(b);

    if (buy_buf) |buys| {
        // Allocate max needed; we will keep "got" semantics using a counter.
        var tmp = allocator.alloc(schema.zp_fifo_snapshot, buys.len) catch return .preparation_fail;
        errdefer allocator.free(tmp);

        var j: usize = 0;
        while (j < buys.len) : (j += 1) {
            const row = buys[j];

            var snap: schema.zp_fifo_snapshot = schema.zp_fifo_snapshot.zero();
            snap.tax_year = year;

            snap.broker = row.broker;
            snap.ticker = row.ticker;
            snap.acq_trade_id = row.id;
            snap.acq_datetime = row.trade_datetime;

            // initial remaining qty is the buy quantity; later step 4 can reduce it
            snap.qty_remaining = row.quantity;

            // cost per share in EUR (you previously used pps * fx)
            snap.cost_per_share_eur = row.price_per_share * row.conversion_rate_eur;

            // commission in EUR (store per-lot; OK for now)
            snap.acq_commission_eur = row.commission * row.conversion_rate_eur;

            snap.country = row.country;

            tmp[j] = snap;
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
                // Only insert lots that are actually positive (defensive)
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
    // 4) For each SELL: count & read open lots (fifo_snapshot) for that broker+ticker up to year
    // ------------------------------------------------------------
    const sells = sell_buf orelse return .internal_error;

    var si: usize = 0;
    while (si < sells.len) : (si += 1) {
        const s = sells[si];

        const broker_z: [*:0]const u8 = @ptrCast(s.broker[0..].ptr);
        const ticker_z: [*:0]const u8 = @ptrCast(s.ticker[0..].ptr);

        const broker_s: [:0]const u8 = std.mem.sliceTo(broker_z, 0);
        const ticker_s: [:0]const u8 = std.mem.sliceTo(ticker_z, 0);

        var open_lot_rows: usize = 0;
        ec = meta.sqlite_count_rows(handle, .fifo_snapshot, year, broker_s, ticker_s, &open_lot_rows);
        if (ec != .ok) return ec;

        if (open_lot_rows == 0) continue;

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

        std.debug.print("There are {d} open lots for {s}\n", .{ got_lots, ticker_s });
    }

    // ------------------------------------------------------------
    // 5) After step 4: insert BUY snapshots that still have positive qty_remaining
    //    (this is where your matching logic will have reduced some buy lots)
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
