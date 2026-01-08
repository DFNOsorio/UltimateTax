const std = @import("std");
const helper = @import("helper.zig");
const schema = @import("schemaStructs.zig");

const fifoSnapshot = @import("fifoSnapshot.zig");
const fifoRealized = @import("fifoRealized.zig");
const insert = @import("insertTrades.zig");
const read = @import("readTrades.zig");
const meta = @import("sqliteMeta.zig");
const processYear = @import("processYear.zig");

pub const c = @cImport({
    @cInclude("sqlite3.h");
});

const DbHandle = helper.DbHandle;

// ------------------------------------------------------------
// Internal helpers (handle open/close)
// ------------------------------------------------------------

fn sqlite_open_handle_impl(path: [*:0]const u8, out_handle: *DbHandle) helper.ErrorCode {
    var db: ?*c.sqlite3 = null;
    const rc = c.sqlite3_open(path, &db);
    if (rc != c.SQLITE_OK or db == null) {
        if (db != null) _ = c.sqlite3_close(db.?);
        return .open_fail;
    }
    out_handle.* = @intFromPtr(db.?);
    return .ok;
}

fn sqlite_close_handle_impl(handle: DbHandle) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    const rc = c.sqlite3_close(db_ptr);
    if (rc != c.SQLITE_OK) return .close_fail;

    return .ok;
}

// ------------------------------------------------------------
// Public Zig-callable API (used by lib.zig exports)
// ------------------------------------------------------------

pub fn zp_sqlite_open(path: [*:0]const u8, out_handle: *DbHandle) helper.ErrorCode {
    return sqlite_open_handle_impl(path, out_handle);
}

pub fn zp_sqlite_close(handle: DbHandle) helper.ErrorCode {
    return sqlite_close_handle_impl(handle);
}

pub fn zp_sqlite_insert_trade(
    handle: DbHandle,
    trade_datetime: ?[*:0]const u8,
    ticker: ?[*:0]const u8,
    quantity: f64,
    price_per_share: f64,
    broker: ?[*:0]const u8,
    trade_type: ?[*:0]const u8,
    commission: f64,
    country: ?[*:0]const u8,
    currency: ?[*:0]const u8,
    conversion_rate_eur: f64,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (trade_datetime == null or ticker == null) return .invalid_argument;

    return insert.sqlite_insert_trade(
        handle,
        trade_datetime.?,
        ticker.?,
        quantity,
        price_per_share,
        broker,
        trade_type,
        commission,
        country,
        currency,
        conversion_rate_eur,
    );
}

pub fn zp_sqlite_insert_trade_struct(
    handle: DbHandle,
    t: ?*const schema.zp_trade,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (t == null) return .invalid_argument;

    return insert.sqlite_insert_trade_struct(handle, t.?);
}

pub fn zp_sqlite_read_trade_by_id(
    handle: DbHandle,
    id: u32,
    out_trade: ?*schema.zp_trade,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_trade == null) return .invalid_argument;

    return read.sqlite_read_trade_by_id(handle, id, out_trade.?);
}

pub fn zp_sqlite_read_trades_by_year(
    handle: DbHandle,
    year: u32,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return read.sqlite_read_trades_by_year(handle, year, out_trades, out_cap, out_count.?);
}

pub fn zp_sqlite_read_trades_by_broker(
    handle: DbHandle,
    broker: ?[*:0]const u8,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (broker == null) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return read.sqlite_read_trades_by_broker(handle, broker.?, out_trades, out_cap, out_count.?);
}

pub fn zp_sqlite_read_trades_by_year_and_broker(
    handle: DbHandle,
    year: u32,
    broker: ?[*:0]const u8,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (broker == null) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return read.sqlite_read_trades_by_year_and_broker(handle, year, broker.?, out_trades, out_cap, out_count.?);
}

pub fn zp_sqlite_read_all_trades(
    handle: DbHandle,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return read.sqlite_read_all_trades(handle, out_trades, out_cap, out_count.?);
}

pub fn zp_sqlite_read_buy_trades_by_year(
    handle: DbHandle,
    year: u32,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return read.sqlite_read_buy_trades_by_year(handle, year, out_trades, out_cap, out_count.?);
}

pub fn zp_sqlite_read_sell_trades_by_year(
    handle: DbHandle,
    year: u32,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return read.sqlite_read_sell_trades_by_year(handle, year, out_trades, out_cap, out_count.?);
}

pub fn zp_sqlite_get_unique_brokers(
    handle: DbHandle,
    out_brokers: ?[*]schema.zp_broker_name,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return meta.sqlite_get_unique_brokers(handle, out_brokers, out_cap, out_count.?);
}

pub fn zp_sqlite_get_unique_years(
    handle: DbHandle,
    out_years: ?[*]u32,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return meta.sqlite_get_unique_years(handle, out_years, out_cap, out_count.?);
}

pub fn zp_sqlite_insert_fifo_snapshot(
    handle: DbHandle,
    row: ?*const schema.zp_fifo_snapshot,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (row == null) return .invalid_argument;

    return fifoSnapshot.sqlite_insert_fifo_snapshot(handle, row.?);
}

pub fn zp_sqlite_read_fifo_snapshot_all(
    handle: DbHandle,
    out_rows: ?[*]schema.zp_fifo_snapshot,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return fifoSnapshot.sqlite_read_fifo_snapshot_all(handle, out_rows, out_cap, out_count.?);
}

pub fn zp_sqlite_read_fifo_snapshot_by_tax_year(
    handle: DbHandle,
    tax_year: u32,
    out_rows: ?[*]schema.zp_fifo_snapshot,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return fifoSnapshot.sqlite_read_fifo_snapshot_by_tax_year(handle, tax_year, out_rows, out_cap, out_count.?);
}

pub fn zp_sqlite_read_fifo_snapshot_by_ticker_per_year(
    handle: DbHandle,
    tax_year: u32,
    ticker: ?[*:0]const u8,
    out_rows: ?[*]schema.zp_fifo_snapshot,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (ticker == null) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return fifoSnapshot.sqlite_read_fifo_snapshot_by_ticker_per_year(handle, tax_year, ticker.?, out_rows, out_cap, out_count.?);
}

pub fn zp_sqlite_read_fifo_snapshot_by_broker_per_year(
    handle: DbHandle,
    tax_year: u32,
    broker: ?[*:0]const u8,
    out_rows: ?[*]schema.zp_fifo_snapshot,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (broker == null) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return fifoSnapshot.sqlite_read_fifo_snapshot_by_broker_per_year(handle, tax_year, broker.?, out_rows, out_cap, out_count.?);
}

pub fn zp_sqlite_read_fifo_snapshot_by_year_broker_ticker(
    handle: DbHandle,
    tax_year: u32,
    broker: [*:0]const u8,
    ticker: [*:0]const u8,
    out_rows: ?[*]schema.zp_fifo_snapshot,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .preparation_fail;

    return fifoSnapshot.sqlite_read_fifo_snapshot_by_year_broker_ticker(
        handle,
        tax_year,
        broker,
        ticker,
        out_rows,
        out_cap,
        out_count.?,
    );
}

pub fn zp_sqlite_delete_fifo_snapshot_by_lot_id(
    handle: DbHandle,
    lot_id: u32,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    return fifoSnapshot.sqlite_delete_fifo_snapshot_by_lot_id(handle, lot_id);
}

pub fn zp_sqlite_update_fifo_snapshot_qty_remaining_by_lot_id(
    handle: DbHandle,
    lot_id: u32,
    qty_remaining: f64,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    return fifoSnapshot.sqlite_update_fifo_snapshot_qty_remaining_by_lot_id(
        handle,
        lot_id,
        qty_remaining,
    );
}

pub fn zp_sqlite_insert_fifo_realized(
    handle: DbHandle,
    row: ?*const schema.zp_fifo_realized,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (row == null) return .invalid_argument;

    return fifoRealized.sqlite_insert_fifo_realized(handle, row.?);
}

pub fn zp_sqlite_read_fifo_realized_all(
    handle: DbHandle,
    out_rows: ?[*]schema.zp_fifo_realized,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return fifoRealized.sqlite_read_fifo_realized_all(handle, out_rows, out_cap, out_count.?);
}

pub fn zp_sqlite_read_fifo_realized_by_tax_year(
    handle: DbHandle,
    tax_year: u32,
    out_rows: ?[*]schema.zp_fifo_realized,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return fifoRealized.sqlite_read_fifo_realized_by_tax_year(handle, tax_year, out_rows, out_cap, out_count.?);
}

pub fn zp_sqlite_read_fifo_realized_by_ticker_per_year(
    handle: DbHandle,
    tax_year: u32,
    ticker: ?[*:0]const u8,
    out_rows: ?[*]schema.zp_fifo_realized,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (ticker == null) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return fifoRealized.sqlite_read_fifo_realized_by_ticker_per_year(handle, tax_year, ticker.?, out_rows, out_cap, out_count.?);
}

pub fn zp_sqlite_read_fifo_realized_by_broker_per_year(
    handle: DbHandle,
    tax_year: u32,
    broker: ?[*:0]const u8,
    out_rows: ?[*]schema.zp_fifo_realized,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (broker == null) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return fifoRealized.sqlite_read_fifo_realized_by_broker_per_year(handle, tax_year, broker.?, out_rows, out_cap, out_count.?);
}

// ------------------------------------------------------------
// Year processing (load-only prototype)
// ------------------------------------------------------------

pub fn zp_sqlite_process_year_load_only(handle: DbHandle, year: u32) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    return processYear.sqlite_process_year_load_only(handle, year);
}

// ------------------------------------------------------------
// COUNT(*) helper
// ------------------------------------------------------------

pub const zp_table = meta.zp_table;

pub fn zp_sqlite_count_rows(
    db: DbHandle,
    table: meta.zp_table,
    year: ?*const u32,
    broker: ?[*:0]const u8,
    ticker: ?[*:0]const u8,
    out_count: ?*usize,
) helper.ErrorCode {
    const y: ?u32 = if (year) |ptr| ptr.* else null;

    const b: ?[:0]const u8 = if (broker) |ptr| std.mem.span(ptr) else null;
    const t: ?[:0]const u8 = if (ticker) |ptr| std.mem.span(ptr) else null;

    if (out_count == null) return helper.ErrorCode.preparation_fail;
    return meta.sqlite_count_rows(db, table, y, b, t, out_count.?);
}

pub fn zp_sqlite_count_buy_trades_by_year(
    db: DbHandle,
    year: u32,
    out_count: ?*usize,
) helper.ErrorCode {
    if (db == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return helper.ErrorCode.preparation_fail;
    return meta.sqlite_count_buy_trades_by_year(db, year, out_count.?);
}

pub fn zp_sqlite_count_sell_trades_by_year(
    db: DbHandle,
    year: u32,
    out_count: ?*usize,
) helper.ErrorCode {
    if (db == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return helper.ErrorCode.preparation_fail;
    return meta.sqlite_count_sell_trades_by_year(db, year, out_count.?);
}
