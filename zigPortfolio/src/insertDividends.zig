const std = @import("std");
const helper = @import("helper.zig");
const schema = @import("schemaStructs.zig");

pub const c = @cImport({
    @cInclude("sqlite3.h");
});

const DbHandle = helper.DbHandle;

const sql_insertion_query: [:0]const u8 =
    "INSERT INTO dividends (broker, dividend_dt, ticker, country, per_share, total_amount, tax, currency, conversion_rate_eur) " ++
    "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);";

fn bind_text(stmt: *c.sqlite3_stmt, position: c_int, text: [*:0]const u8) helper.ErrorCode {
    const rc: c_int = c.sqlite3_bind_text(stmt, position, text, -1, c.SQLITE_TRANSIENT);
    return if (rc == c.SQLITE_OK) .ok else .insertion_error;
}

fn bind_real(stmt: *c.sqlite3_stmt, position: c_int, value: f64) helper.ErrorCode {
    const rc: c_int = c.sqlite3_bind_double(stmt, position, value);
    return if (rc == c.SQLITE_OK) .ok else .insertion_error;
}

fn is_empty_cstr(buf: []const u8) bool {
    return buf.len == 0 or buf[0] == 0;
}

fn cstr_ptr(buf: []const u8) [*:0]const u8 {
    // Precondition: buf is NUL-terminated
    return @ptrCast(buf.ptr);
}

fn bind_required_buf_text(stmt: *c.sqlite3_stmt, pos: c_int, buf: []const u8) helper.ErrorCode {
    if (is_empty_cstr(buf)) return .invalid_argument;
    return bind_text(stmt, pos, cstr_ptr(buf));
}

fn validate_required_real_pos(v: f64) helper.ErrorCode {
    if (!std.math.isFinite(v) or v <= 0.0) return .invalid_argument;
    return .ok;
}

fn validate_required_real_nonneg(v: f64) helper.ErrorCode {
    if (!std.math.isFinite(v) or v < 0.0) return .invalid_argument;
    return .ok;
}

pub fn sqlite_insert_dividend(
    handle: DbHandle,
    broker: [*:0]const u8,
    dividend_dt: [*:0]const u8,
    ticker: [*:0]const u8,
    country: [*:0]const u8,
    per_share: f64,
    total_amount: f64,
    tax: f64,
    currency: [*:0]const u8,
    conversion_rate_eur: f64,
) helper.ErrorCode {
    // Numeric validation (schema requires NOT NULL; enforce sane domain)
    var rcg = validate_required_real_pos(per_share);
    if (rcg != .ok) return rcg;

    rcg = validate_required_real_nonneg(total_amount);
    if (rcg != .ok) return rcg;

    rcg = validate_required_real_nonneg(tax);
    if (rcg != .ok) return rcg;

    rcg = validate_required_real_pos(conversion_rate_eur);
    if (rcg != .ok) return rcg;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    var rc: c_int = c.sqlite3_prepare_v2(db_ptr, sql_insertion_query.ptr, -1, &stmt, null);
    if (rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const s = stmt.?;

    var rcc: helper.ErrorCode = .ok;

    rcc = bind_text(s, 1, broker);
    if (rcc != .ok) return rcc;

    rcc = bind_text(s, 2, dividend_dt);
    if (rcc != .ok) return rcc;

    rcc = bind_text(s, 3, ticker);
    if (rcc != .ok) return rcc;

    rcc = bind_text(s, 4, country);
    if (rcc != .ok) return rcc;

    rcc = bind_real(s, 5, per_share);
    if (rcc != .ok) return rcc;

    rcc = bind_real(s, 6, total_amount);
    if (rcc != .ok) return rcc;

    rcc = bind_real(s, 7, tax);
    if (rcc != .ok) return rcc;

    rcc = bind_text(s, 8, currency);
    if (rcc != .ok) return rcc;

    rcc = bind_real(s, 9, conversion_rate_eur);
    if (rcc != .ok) return rcc;

    rc = c.sqlite3_step(s);
    if (rc != c.SQLITE_DONE) return .execution_fail;

    return .ok;
}

/// Struct-based insert (inline buffers, required fields only)
pub fn sqlite_insert_dividend_struct(
    handle: DbHandle,
    d: *const schema.zp_dividend,
) helper.ErrorCode {
    // Numeric validation
    var rcg = validate_required_real_pos(d.per_share);
    if (rcg != .ok) return rcg;

    rcg = validate_required_real_nonneg(d.total_amount);
    if (rcg != .ok) return rcg;

    rcg = validate_required_real_nonneg(d.tax);
    if (rcg != .ok) return rcg;

    rcg = validate_required_real_pos(d.conversion_rate_eur);
    if (rcg != .ok) return rcg;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    var rc: c_int = c.sqlite3_prepare_v2(db_ptr, sql_insertion_query.ptr, -1, &stmt, null);
    if (rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const s = stmt.?;

    var rcc: helper.ErrorCode = .ok;

    rcc = bind_required_buf_text(s, 1, d.broker[0..]);
    if (rcc != .ok) return rcc;

    rcc = bind_required_buf_text(s, 2, d.dividend_dt[0..]);
    if (rcc != .ok) return rcc;

    rcc = bind_required_buf_text(s, 3, d.ticker[0..]);
    if (rcc != .ok) return rcc;

    rcc = bind_required_buf_text(s, 4, d.country[0..]);
    if (rcc != .ok) return rcc;

    rcc = bind_real(s, 5, d.per_share);
    if (rcc != .ok) return rcc;

    rcc = bind_real(s, 6, d.total_amount);
    if (rcc != .ok) return rcc;

    rcc = bind_real(s, 7, d.tax);
    if (rcc != .ok) return rcc;

    rcc = bind_required_buf_text(s, 8, d.currency[0..]);
    if (rcc != .ok) return rcc;

    rcc = bind_real(s, 9, d.conversion_rate_eur);
    if (rcc != .ok) return rcc;

    rc = c.sqlite3_step(s);
    if (rc != c.SQLITE_DONE) return .execution_fail;

    return .ok;
}
