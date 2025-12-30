const std = @import("std");

const helper = @import("helper.zig");
const trade_mod = @import("trade.zig");

const DbHandle = helper.DbHandle;

const zp_trade = trade_mod.zp_trade;

pub const c = @cImport({
    @cInclude("sqlite3.h");
});

const libc = @cImport({
    @cInclude("stdlib.h");
});

const sql_insertion_query: [:0]const u8 =
    "INSERT INTO trades (broker, trade_datetime, type, ticker, quantity, price_per_share, commission, country, currency, conversion_rate_eur) " ++ "VALUES (" ++ "COALESCE(?, 'IKBR'), " ++ "?, " ++ "COALESCE(?, 'BUY'), " ++ "?, " ++ "?, " ++ "?, " ++ "COALESCE(?, 0.0), " ++ "COALESCE(?, 'US'), " ++ "COALESCE(?, 'USD'), " ++ "COALESCE(?, 1.0)" ++ ");";

const sql_select_trade_by_id: [:0]const u8 =
    "SELECT broker, trade_datetime, \"type\", ticker, " ++ "quantity, price_per_share, commission, " ++ "country, currency, conversion_rate_eur " ++ "FROM trades WHERE id = ? LIMIT 1;";

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

fn dupColumnTextZ(stmt: *c.sqlite3_stmt, col: c_int) ?[*:0]const u8 {
    const p = c.sqlite3_column_text(stmt, col);
    if (p == null) return null;

    const n: usize = @intCast(c.sqlite3_column_bytes(stmt, col));
    const mem = libc.malloc(n + 1) orelse return null;

    const dst: [*]u8 = @ptrCast(mem);
    const src: [*]const u8 = @ptrCast(p.?);

    std.mem.copyForwards(u8, dst[0..n], src[0..n]);
    dst[n] = 0;

    return @as([*:0]const u8, @ptrCast(dst));
}

fn freeCstr(p: ?[*:0]const u8) void {
    if (p) |q| {
        // free expects mutable void*; constCast is fine since we only free.
        libc.free(@ptrCast(@constCast(q)));
    }
}

pub fn trade_free(tr: *zp_trade) void {
    freeCstr(tr.trade_datetime);
    freeCstr(tr.ticker);
    freeCstr(tr.broker);
    freeCstr(tr.type);
    freeCstr(tr.country);
    freeCstr(tr.currency);

    // leave numerics untouched; reset pointers to null for safety
    tr.trade_datetime = null;
    tr.ticker = null;
    tr.broker = null;
    tr.type = null;
    tr.country = null;
    tr.currency = null;
}

pub fn sqlite_read_trade_by_id(handle: DbHandle, id: u32, out_trade: *zp_trade) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    // Ensure out_trade is in a known state.
    // NOTE: this does NOT free existing pointers; caller should call zp_trade_free if reusing.
    out_trade.* = std.mem.zeroes(zp_trade);

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    var rc: c_int = c.sqlite3_prepare_v2(db_ptr, sql_select_trade_by_id.ptr, -1, &stmt, null);
    if (rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    rc = c.sqlite3_bind_int64(stmt.?, 1, @as(i64, id));
    if (rc != c.SQLITE_OK) return .read_row_fail;

    rc = c.sqlite3_step(stmt.?);

    if (rc == c.SQLITE_DONE) {
        // Not found: out_trade stays zeroed (all pointers null)
        return .ok;
    }

    if (rc != c.SQLITE_ROW) {
        return .read_row_fail;
    }

    // Allocate and copy all TEXT columns so the data survives finalize()
    out_trade.broker = dupColumnTextZ(stmt.?, 0) orelse return .internal_error;
    errdefer trade_free(out_trade);

    out_trade.trade_datetime = dupColumnTextZ(stmt.?, 1) orelse return .internal_error;
    out_trade.type = dupColumnTextZ(stmt.?, 2) orelse return .internal_error;
    out_trade.ticker = dupColumnTextZ(stmt.?, 3) orelse return .internal_error;

    // Numerics
    out_trade.quantity = c.sqlite3_column_double(stmt.?, 4);
    out_trade.price_per_share = c.sqlite3_column_double(stmt.?, 5);
    out_trade.commission = c.sqlite3_column_double(stmt.?, 6);

    out_trade.country = dupColumnTextZ(stmt.?, 7) orelse return .internal_error;
    out_trade.currency = dupColumnTextZ(stmt.?, 8) orelse return .internal_error;

    out_trade.conversion_rate_eur = c.sqlite3_column_double(stmt.?, 9);

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
