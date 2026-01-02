const std = @import("std");
const helper = @import("helper.zig");
const trade = @import("trade.zig");

pub const c = @cImport({
    @cInclude("sqlite3.h");
});

const DbHandle = helper.DbHandle;

const sql_read_by_id: [:0]const u8 =
    \\SELECT broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\WHERE id = ?1
    \\LIMIT 1;
;

const sql_read_by_year: [:0]const u8 =
    \\SELECT broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\WHERE substr(trade_datetime, 1, 4) = ?1
    \\ORDER BY trade_datetime ASC, id ASC;
;

const sql_read_by_broker: [:0]const u8 =
    \\SELECT broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\WHERE broker = ?1
    \\ORDER BY trade_datetime ASC, id ASC;
;

const sql_read_by_year_and_broker: [:0]const u8 =
    \\SELECT broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\WHERE substr(trade_datetime, 1, 4) = ?1
    \\  AND broker = ?2
    \\ORDER BY trade_datetime ASC, id ASC;
;

const sql_read_all: [:0]const u8 =
    \\SELECT broker, trade_datetime, "type", ticker,
    \\       quantity, price_per_share, commission,
    \\       country, currency, conversion_rate_eur
    \\FROM trades
    \\ORDER BY trade_datetime ASC, id ASC;
;

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

fn stmt_to_trade(stmt: *c.sqlite3_stmt, out: *trade.zp_trade) void {
    copy_col_text_into(out.broker[0..], stmt, 0);
    copy_col_text_into(out.trade_datetime[0..], stmt, 1);
    copy_col_text_into(out.trade_type[0..], stmt, 2);
    copy_col_text_into(out.ticker[0..], stmt, 3);

    out.quantity = c.sqlite3_column_double(stmt, 4);
    out.price_per_share = c.sqlite3_column_double(stmt, 5);
    out.commission = c.sqlite3_column_double(stmt, 6);

    copy_col_text_into(out.country[0..], stmt, 7);
    copy_col_text_into(out.currency[0..], stmt, 8);

    out.conversion_rate_eur = c.sqlite3_column_double(stmt, 9);
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
    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_by_id.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const rc_bind: c_int = c.sqlite3_bind_int(stmt.?, 1, @as(c_int, @intCast(id)));
    if (rc_bind != c.SQLITE_OK) return .preparation_fail;

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
    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_by_year.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    var year_buf: [5]u8 = undefined;
    _ = std.fmt.bufPrintZ(&year_buf, "{d:0>4}", .{year}) catch return .invalid_argument;

    const rc_bind: c_int = c.sqlite3_bind_text(stmt.?, 1, @ptrCast(year_buf[0..].ptr), -1, c.SQLITE_TRANSIENT);
    if (rc_bind != c.SQLITE_OK) return .preparation_fail;

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
    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_by_year_and_broker.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    var year_buf: [5]u8 = undefined;
    _ = std.fmt.bufPrintZ(&year_buf, "{d:0>4}", .{year}) catch return .invalid_argument;

    var rc_bind: c_int = c.sqlite3_bind_text(stmt.?, 1, @ptrCast(year_buf[0..].ptr), -1, c.SQLITE_TRANSIENT);
    if (rc_bind != c.SQLITE_OK) return .preparation_fail;

    rc_bind = c.sqlite3_bind_text(stmt.?, 2, broker, -1, c.SQLITE_TRANSIENT);
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
    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_all.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    return read_trades_loop(stmt.?, out_trades, out_cap, out_count);
}
