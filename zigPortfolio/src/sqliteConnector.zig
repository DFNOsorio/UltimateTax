const std = @import("std");
const helper = @import("helper.zig");

pub const c = @cImport({
    @cInclude("sqlite3.h");
});

const DbHandle = helper.DbHandle;

const sql_insertion_query: [:0]const u8 =
    "INSERT INTO trades (broker, trade_datetime, type, ticker, quantity, price_per_share, commission, country, currency, conversion_rate_eur) " ++ "VALUES (" ++ "COALESCE(?, 'IKBR'), " ++ "?, " ++ "COALESCE(?, 'BUY'), " ++ "?, " ++ "?, " ++ "?, " ++ "COALESCE(?, 0.0), " ++ "COALESCE(?, 'US'), " ++ "COALESCE(?, 'USD'), " ++ "COALESCE(?, 1.0)" ++ ");";

pub fn sqlite_hello_impl() void {
    std.debug.print("Hello from sqliteConnector!\n", .{});
}

/// Open a DB and return an opaque handle (pointer-as-handle internally).
pub fn sqlite_open_handle_impl(path: [*:0]const u8, out_handle: *DbHandle) helper.ErrorCode {
    // Precondition: path and out_handle are non-null (C contract)

    var db: ?*c.sqlite3 = null;
    const rc = c.sqlite3_open(path, &db);
    if (rc != c.SQLITE_OK or db == null) {
        if (db != null) {
            _ = c.sqlite3_close(db.?);
        }
        return .open_fail;
    }

    // Convert pointer to integer handle
    out_handle.* = @intFromPtr(db.?);
    return .ok;
}

fn insert_text(stmt: *c.sqlite3_stmt, position: c_int, text: [*:0]const u8) helper.ErrorCode {
    const rc: c_int = c.sqlite3_bind_text(stmt, position, text, -1, c.SQLITE_TRANSIENT);
    return if (rc == c.SQLITE_OK) .ok else .insertion_error;
}

fn insert_real(stmt: *c.sqlite3_stmt, position: c_int, value: f64) helper.ErrorCode {
    const rc: c_int = c.sqlite3_bind_double(stmt, position, value);
    return if (rc == c.SQLITE_OK) .ok else .insertion_error;
}

fn insert_text_opt(stmt: *c.sqlite3_stmt, position: c_int, text: ?[*:0]const u8) helper.ErrorCode {
    const rc: c_int = if (text) |t|
        c.sqlite3_bind_text(stmt, position, t, -1, c.SQLITE_TRANSIENT)
    else
        c.sqlite3_bind_null(stmt, position);

    return if (rc == c.SQLITE_OK) .ok else .insertion_error;
}

fn insert_real_opt(stmt: *c.sqlite3_stmt, position: c_int, value: ?f64) helper.ErrorCode {
    const rc: c_int = if (value) |v|
        c.sqlite3_bind_double(stmt, position, v)
    else
        c.sqlite3_bind_null(stmt, position);

    return if (rc == c.SQLITE_OK) .ok else .insertion_error;
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

    var rc: c_int = c.sqlite3_prepare_v2(db_ptr, sql_insertion_query, -1, &stmt, null);
    if (rc != c.SQLITE_OK or stmt == null) return .preparation_fail;

    defer _ = c.sqlite3_finalize(stmt.?);
    const s = stmt.?;

    // Normalize numeric “defaults”:
    const comm_opt: ?f64 = if (!std.math.isFinite(commission) or commission < 0.0) null else commission;
    const conv_opt: ?f64 = if (!std.math.isFinite(conversion_rate_eur) or conversion_rate_eur <= 0.0) null else conversion_rate_eur;

    var rcc: helper.ErrorCode = insert_text_opt(s, 1, broker);
    if (rcc != .ok) return rcc;

    rcc = insert_text(s, 2, trade_datetime);
    if (rcc != .ok) return rcc;

    rcc = insert_text_opt(s, 3, trade_type);
    if (rcc != .ok) return rcc;

    rcc = insert_text(s, 4, ticker);
    if (rcc != .ok) return rcc;

    rcc = insert_real(s, 5, quantity);
    if (rcc != .ok) return rcc;

    rcc = insert_real(s, 6, price_per_share);
    if (rcc != .ok) return rcc;

    rcc = insert_real_opt(s, 7, comm_opt);
    if (rcc != .ok) return rcc;

    rcc = insert_text_opt(s, 8, country);
    if (rcc != .ok) return rcc;

    rcc = insert_text_opt(s, 9, currency);
    if (rcc != .ok) return rcc;

    rcc = insert_real_opt(s, 10, conv_opt);
    if (rcc != .ok) return rcc;

    rc = c.sqlite3_step(s);
    if (rc != c.SQLITE_DONE) return .execution_fail;

    return .ok;
}

/// Close a DB given an opaque handle.
pub fn sqlite_close_handle_impl(handle: DbHandle) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) {
        return .invalid_argument;
    }

    // Zig 0.15.2: @ptrFromInt takes ONE argument, type comes from context
    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);

    const rc = c.sqlite3_close(db_ptr);
    if (rc != c.SQLITE_OK) {
        return .close_fail;
    }

    return .ok;
}
