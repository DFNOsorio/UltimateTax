const std = @import("std");
const helper = @import("helper.zig");
const trade = @import("schemaStructs.zig");

pub const c = @cImport({
    @cInclude("sqlite3.h");
});

const DbHandle = helper.DbHandle;

// ------------------------------------------------------------
// SQL (include `id` as column 0 so zp_trade.id is always populated)
// ------------------------------------------------------------

const sql_read_by_id: [:0]const u8 =
    \\SELECT id, broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\WHERE id = ?1
    \\LIMIT 1;
;

const sql_read_by_year: [:0]const u8 =
    \\SELECT id, broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\WHERE substr(trade_datetime, 1, 4) = ?1
    \\ORDER BY trade_datetime ASC, id ASC;
;

const sql_read_by_broker: [:0]const u8 =
    \\SELECT id, broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\WHERE broker = ?1
    \\ORDER BY trade_datetime ASC, id ASC;
;

const sql_read_by_year_and_broker: [:0]const u8 =
    \\SELECT id, broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\WHERE substr(trade_datetime, 1, 4) = ?1
    \\  AND broker = ?2
    \\ORDER BY trade_datetime ASC, id ASC;
;

const sql_read_all: [:0]const u8 =
    \\SELECT id, broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\ORDER BY trade_datetime ASC, id ASC;
;

const sql_read_buy_by_year: [:0]const u8 =
    \\SELECT id, broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\WHERE substr(trade_datetime, 1, 4) = ?1
    \\  AND upper("type") = 'BUY'
    \\ORDER BY trade_datetime ASC, id ASC;
;

const sql_read_sell_by_year: [:0]const u8 =
    \\SELECT id, broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\WHERE substr(trade_datetime, 1, 4) = ?1
    \\  AND upper("type") = 'SELL'
    \\ORDER BY trade_datetime ASC, id ASC;
;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

fn copy_col_text_into(buf: []u8, stmt: *c.sqlite3_stmt, col: c_int) void {
    @memset(buf, 0);

    const p = c.sqlite3_column_text(stmt, col);
    if (p == null) return;

    const n_bytes: usize = @intCast(c.sqlite3_column_bytes(stmt, col));
    const src: [*]const u8 = @ptrCast(p.?);

    if (buf.len == 0) return;

    const to_copy = @min(n_bytes, buf.len - 1);
    std.mem.copyForwards(u8, buf[0..to_copy], src[0..to_copy]);
    buf[to_copy] = 0;
}

fn bind_u32(stmt: *c.sqlite3_stmt, idx: c_int, v: u32) helper.ErrorCode {
    // Avoid narrowing panic: bind as int64.
    const rc = c.sqlite3_bind_int64(stmt, idx, @as(c.sqlite3_int64, @intCast(v)));
    return if (rc == c.SQLITE_OK) .ok else .preparation_fail;
}

fn bind_year_text(stmt: *c.sqlite3_stmt, idx: c_int, year: u32) helper.ErrorCode {
    var year_buf: [5]u8 = undefined;
    _ = std.fmt.bufPrintZ(&year_buf, "{d:0>4}", .{year}) catch return .invalid_argument;

    const rc: c_int =
        c.sqlite3_bind_text(stmt, idx, @ptrCast(year_buf[0..].ptr), -1, c.SQLITE_TRANSIENT);
    return if (rc == c.SQLITE_OK) .ok else .preparation_fail;
}

fn stmt_to_trade(stmt: *c.sqlite3_stmt, out: *trade.zp_trade) void {
    // Column 0 is always id
    const id_i64 = c.sqlite3_column_int64(stmt, 0);
    out.id = @as(u32, @intCast(@max(@as(i64, 0), @as(i64, @intCast(id_i64)))));

    copy_col_text_into(out.broker[0..], stmt, 1);
    copy_col_text_into(out.trade_datetime[0..], stmt, 2);
    copy_col_text_into(out.trade_type[0..], stmt, 3);
    copy_col_text_into(out.ticker[0..], stmt, 4);

    out.quantity = c.sqlite3_column_double(stmt, 5);
    out.price_per_share = c.sqlite3_column_double(stmt, 6);
    out.commission = c.sqlite3_column_double(stmt, 7);

    copy_col_text_into(out.country[0..], stmt, 8);
    copy_col_text_into(out.currency[0..], stmt, 9);

    out.conversion_rate_eur = c.sqlite3_column_double(stmt, 10);
}

fn read_trades_loop(
    stmt: *c.sqlite3_stmt,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    out_count.* = 0;

    var idx: usize = 0;
    while (true) {
        const rc: c_int = c.sqlite3_step(stmt);
        if (rc == c.SQLITE_ROW) {
            if (out_trades) |buf| {
                if (idx >= out_cap) {
                    out_count.* = idx;
                    return .ok; // truncated safely
                }
                stmt_to_trade(stmt, &buf[idx]);
            }
            idx += 1;
            continue;
        } else if (rc == c.SQLITE_DONE) {
            out_count.* = idx;
            return .ok;
        } else {
            return .read_row_fail;
        }
    }
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------

/// Read exactly one trade by DB id.
/// Returns:
/// - .ok if found and written
/// - .execution_fail if not found
/// - .read_row_fail if sqlite reported an error executing the query
pub fn sqlite_read_trade_by_id(
    handle: DbHandle,
    id: u32,
    out_trade: *trade.zp_trade,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_by_id.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    // bind id (int64 to avoid narrowing issues)
    const ec = bind_u32(stmt.?, 1, id);
    if (ec != .ok) return ec;

    const rc_step: c_int = c.sqlite3_step(stmt.?);
    if (rc_step == c.SQLITE_ROW) {
        stmt_to_trade(stmt.?, out_trade);
        return .ok;
    } else if (rc_step == c.SQLITE_DONE) {
        return .execution_fail; // not found
    } else {
        return .read_row_fail;
    }
}

/// Read all trades for a given year.
/// If out_trades == null and out_cap == 0, returns the required count in out_count.
/// Otherwise, writes up to out_cap trades and sets out_count to the number written.
pub fn sqlite_read_trades_by_year(
    handle: DbHandle,
    year: u32,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_by_year.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const ec = bind_year_text(stmt.?, 1, year);
    if (ec != .ok) return ec;

    return read_trades_loop(stmt.?, out_trades, out_cap, out_count);
}

/// Read all trades for a given broker.
pub fn sqlite_read_trades_by_broker(
    handle: DbHandle,
    broker: [*:0]const u8,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_by_broker.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const rc_bind: c_int = c.sqlite3_bind_text(stmt.?, 1, broker, -1, c.SQLITE_TRANSIENT);
    if (rc_bind != c.SQLITE_OK) return .preparation_fail;

    return read_trades_loop(stmt.?, out_trades, out_cap, out_count);
}

/// Read all trades for a given year and broker.
pub fn sqlite_read_trades_by_year_and_broker(
    handle: DbHandle,
    year: u32,
    broker: [*:0]const u8,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_by_year_and_broker.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    var ec: helper.ErrorCode = .ok;

    ec = bind_year_text(stmt.?, 1, year);
    if (ec != .ok) return ec;

    const rc_bind: c_int = c.sqlite3_bind_text(stmt.?, 2, broker, -1, c.SQLITE_TRANSIENT);
    if (rc_bind != c.SQLITE_OK) return .preparation_fail;

    return read_trades_loop(stmt.?, out_trades, out_cap, out_count);
}

/// Read all trades in the table.
pub fn sqlite_read_all_trades(
    handle: DbHandle,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_all.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    return read_trades_loop(stmt.?, out_trades, out_cap, out_count);
}

/// Read all BUY trades for a given year.
/// If out_trades == null and out_cap == 0, returns the required count in out_count.
/// Otherwise, writes up to out_cap trades and sets out_count to the number written.
pub fn sqlite_read_buy_trades_by_year(
    handle: DbHandle,
    year: u32,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_buy_by_year.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const ec = bind_year_text(stmt.?, 1, year);
    if (ec != .ok) return ec;

    return read_trades_loop(stmt.?, out_trades, out_cap, out_count);
}

/// Read all SELL trades for a given year.
/// If out_trades == null and out_cap == 0, returns the required count in out_count.
/// Otherwise, writes up to out_cap trades and sets out_count to the number written.
pub fn sqlite_read_sell_trades_by_year(
    handle: DbHandle,
    year: u32,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_sell_by_year.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const ec = bind_year_text(stmt.?, 1, year);
    if (ec != .ok) return ec;

    return read_trades_loop(stmt.?, out_trades, out_cap, out_count);
}
