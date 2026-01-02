const std = @import("std");
const helper = @import("helper.zig");
const trade = @import("schemaStructs.zig");

pub const c = @cImport({
    @cInclude("sqlite3.h");
});

const DbHandle = helper.DbHandle;

const sql_insertion_query: [:0]const u8 =
    "INSERT INTO trades (broker, trade_datetime, type, ticker, quantity, price_per_share, commission, country, currency, conversion_rate_eur) " ++
    "VALUES (COALESCE(?1, 'IKBR'), ?2, COALESCE(?3, 'BUY'), ?4, ?5, ?6, COALESCE(?7, 0.0), COALESCE(?8, 'US'), COALESCE(?9, 'USD'), COALESCE(?10, 1.0));";

fn bind_text(stmt: *c.sqlite3_stmt, position: c_int, text: [*:0]const u8) helper.ErrorCode {
    const rc: c_int = c.sqlite3_bind_text(stmt, position, text, -1, c.SQLITE_TRANSIENT);
    return if (rc == c.SQLITE_OK) .ok else .insertion_error;
}

fn bind_real(stmt: *c.sqlite3_stmt, position: c_int, value: f64) helper.ErrorCode {
    const rc: c_int = c.sqlite3_bind_double(stmt, position, value);
    return if (rc == c.SQLITE_OK) .ok else .insertion_error;
}

fn bind_text_opt(stmt: *c.sqlite3_stmt, position: c_int, text: ?[*:0]const u8) helper.ErrorCode {
    const rc: c_int = if (text) |t|
        c.sqlite3_bind_text(stmt, position, t, -1, c.SQLITE_TRANSIENT)
    else
        c.sqlite3_bind_null(stmt, position);

    return if (rc == c.SQLITE_OK) .ok else .insertion_error;
}

fn bind_real_opt(stmt: *c.sqlite3_stmt, position: c_int, value: ?f64) helper.ErrorCode {
    const rc: c_int = if (value) |v|
        c.sqlite3_bind_double(stmt, position, v)
    else
        c.sqlite3_bind_null(stmt, position);

    return if (rc == c.SQLITE_OK) .ok else .insertion_error;
}

fn is_empty_cstr(buf: []const u8) bool {
    return buf.len == 0 or buf[0] == 0;
}

fn cstr_ptr(buf: []const u8) [*:0]const u8 {
    // Precondition: buf is NUL-terminated
    return @ptrCast(buf.ptr);
}

fn bind_trade_text_defaultable(stmt: *c.sqlite3_stmt, pos: c_int, buf: []const u8) helper.ErrorCode {
    // Empty inline string => use default => bind NULL so COALESCE fires
    if (is_empty_cstr(buf)) return bind_text_opt(stmt, pos, null);
    return bind_text(stmt, pos, cstr_ptr(buf));
}

fn bind_trade_text_required(stmt: *c.sqlite3_stmt, pos: c_int, buf: []const u8) helper.ErrorCode {
    if (is_empty_cstr(buf)) return .invalid_argument;
    return bind_text(stmt, pos, cstr_ptr(buf));
}

fn normalize_commission(v: f64) ?f64 {
    if (!std.math.isFinite(v) or v < 0.0) return null;
    return v;
}

fn normalize_conversion(v: f64) ?f64 {
    if (!std.math.isFinite(v) or v <= 0.0) return null;
    return v;
}

pub fn sqlite_insert_trade(
    handle: DbHandle,
    trade_datetime: [*:0]const u8,
    ticker: [*:0]const u8,
    quantity: f64,
    price_per_share: f64,
    broker: ?[*:0]const u8,
    trade_type: ?[*:0]const u8,
    commission: f64,
    country: ?[*:0]const u8,
    currency: ?[*:0]const u8,
    conversion_rate_eur: f64,
) helper.ErrorCode {
    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    var rc: c_int = c.sqlite3_prepare_v2(db_ptr, sql_insertion_query.ptr, -1, &stmt, null);
    if (rc != c.SQLITE_OK or stmt == null) return .preparation_fail;

    defer _ = c.sqlite3_finalize(stmt.?);
    const s = stmt.?;

    const comm_opt: ?f64 = normalize_commission(commission);
    const conv_opt: ?f64 = normalize_conversion(conversion_rate_eur);

    var rcc: helper.ErrorCode = bind_text_opt(s, 1, broker);
    if (rcc != .ok) return rcc;

    rcc = bind_text(s, 2, trade_datetime);
    if (rcc != .ok) return rcc;

    rcc = bind_text_opt(s, 3, trade_type);
    if (rcc != .ok) return rcc;

    rcc = bind_text(s, 4, ticker);
    if (rcc != .ok) return rcc;

    rcc = bind_real(s, 5, quantity);
    if (rcc != .ok) return rcc;

    rcc = bind_real(s, 6, price_per_share);
    if (rcc != .ok) return rcc;

    rcc = bind_real_opt(s, 7, comm_opt);
    if (rcc != .ok) return rcc;

    rcc = bind_text_opt(s, 8, country);
    if (rcc != .ok) return rcc;

    rcc = bind_text_opt(s, 9, currency);
    if (rcc != .ok) return rcc;

    rcc = bind_real_opt(s, 10, conv_opt);
    if (rcc != .ok) return rcc;

    rc = c.sqlite3_step(s);
    if (rc != c.SQLITE_DONE) return .execution_fail;

    return .ok;
}

/// Struct-based insert (the struct uses inline buffers).
pub fn sqlite_insert_trade_struct(
    handle: DbHandle,
    t: *const trade.zp_trade,
) helper.ErrorCode {
    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    var rc: c_int = c.sqlite3_prepare_v2(db_ptr, sql_insertion_query.ptr, -1, &stmt, null);
    if (rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);
    const s = stmt.?;

    // Required
    var rcc = bind_trade_text_required(s, 2, t.trade_datetime[0..]);
    if (rcc != .ok) return rcc;

    rcc = bind_trade_text_required(s, 4, t.ticker[0..]);
    if (rcc != .ok) return rcc;

    // Required numerics
    rcc = bind_real(s, 5, t.quantity);
    if (rcc != .ok) return rcc;

    rcc = bind_real(s, 6, t.price_per_share);
    if (rcc != .ok) return rcc;

    // Defaultables (bind NULL => COALESCE uses DB defaults)
    rcc = bind_trade_text_defaultable(s, 1, t.broker[0..]);
    if (rcc != .ok) return rcc;

    rcc = bind_trade_text_defaultable(s, 3, t.trade_type[0..]);
    if (rcc != .ok) return rcc;

    rcc = bind_real_opt(s, 7, normalize_commission(t.commission));
    if (rcc != .ok) return rcc;

    rcc = bind_trade_text_defaultable(s, 8, t.country[0..]);
    if (rcc != .ok) return rcc;

    rcc = bind_trade_text_defaultable(s, 9, t.currency[0..]);
    if (rcc != .ok) return rcc;

    rcc = bind_real_opt(s, 10, normalize_conversion(t.conversion_rate_eur));
    if (rcc != .ok) return rcc;

    rc = c.sqlite3_step(s);
    if (rc != c.SQLITE_DONE) return .execution_fail;

    return .ok;
}
