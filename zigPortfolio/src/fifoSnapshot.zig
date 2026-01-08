const std = @import("std");
const helper = @import("helper.zig");
const schema = @import("schemaStructs.zig");

pub const c = @cImport({
    @cInclude("sqlite3.h");
});

const DbHandle = helper.DbHandle;
const Row = schema.zp_fifo_snapshot;

fn cstr_required(buf: []const u8) ?[*:0]const u8 {
    // expects inline NUL-terminated buffer; empty => invalid
    if (buf.len == 0 or buf[0] == 0) return null;
    return @ptrCast(buf.ptr);
}

fn bind_text(stmt: *c.sqlite3_stmt, idx: c_int, s: [*:0]const u8) helper.ErrorCode {
    const rc = c.sqlite3_bind_text(stmt, idx, s, -1, c.SQLITE_TRANSIENT);
    return if (rc == c.SQLITE_OK) .ok else .preparation_fail;
}

fn bind_u32(stmt: *c.sqlite3_stmt, idx: c_int, v: u32) helper.ErrorCode {
    const rc = c.sqlite3_bind_int(stmt, idx, @as(c_int, @intCast(v)));
    return if (rc == c.SQLITE_OK) .ok else .preparation_fail;
}

fn bind_f64(stmt: *c.sqlite3_stmt, idx: c_int, v: f64) helper.ErrorCode {
    const rc = c.sqlite3_bind_double(stmt, idx, v);
    return if (rc == c.SQLITE_OK) .ok else .preparation_fail;
}

fn bind_nullable_f64_default0(stmt: *c.sqlite3_stmt, idx: c_int, v: f64) helper.ErrorCode {
    // NaN or < 0 => bind NULL so DB default (0.0) can apply via COALESCE
    const rc: c_int = if (std.math.isNan(v) or v < 0.0)
        c.sqlite3_bind_null(stmt, idx)
    else
        c.sqlite3_bind_double(stmt, idx, v);

    return if (rc == c.SQLITE_OK) .ok else .preparation_fail;
}

fn copy_col_text_into(dst: []u8, stmt: *c.sqlite3_stmt, col: c_int) void {
    @memset(dst, 0);

    const p = c.sqlite3_column_text(stmt, col);
    if (p == null) return;

    const n_bytes: usize = @intCast(c.sqlite3_column_bytes(stmt, col));
    const src: [*]const u8 = @ptrCast(p.?);

    if (dst.len == 0) return;
    const to_copy = @min(n_bytes, dst.len - 1);
    std.mem.copyForwards(u8, dst[0..to_copy], src[0..to_copy]);
    dst[to_copy] = 0;
}

fn stmt_to_row(stmt: *c.sqlite3_stmt, out: *Row) void {
    // Column order must match SELECTs below
    out.lot_id = @as(u32, @intCast(c.sqlite3_column_int(stmt, 0)));

    copy_col_text_into(out.broker[0..], stmt, 1);
    out.tax_year = @as(u32, @intCast(c.sqlite3_column_int(stmt, 2)));
    copy_col_text_into(out.ticker[0..], stmt, 3);

    out.acq_trade_id = @as(u32, @intCast(c.sqlite3_column_int(stmt, 4)));
    copy_col_text_into(out.acq_datetime[0..], stmt, 5);

    out.qty_remaining = c.sqlite3_column_double(stmt, 6);
    out.cost_per_share_eur = c.sqlite3_column_double(stmt, 7);
    out.acq_commission_eur = c.sqlite3_column_double(stmt, 8);

    copy_col_text_into(out.country[0..], stmt, 9);
}

fn read_loop(
    stmt: *c.sqlite3_stmt,
    out_rows: ?[*]Row,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    var idx: usize = 0;

    while (true) {
        const rc = c.sqlite3_step(stmt);
        if (rc == c.SQLITE_ROW) {
            if (out_rows) |buf| {
                if (idx >= out_cap) {
                    out_count.* = idx; // truncated safely
                    return .ok;
                }
                stmt_to_row(stmt, &buf[idx]);
            }
            idx += 1;
            continue;
        }

        if (rc == c.SQLITE_DONE) {
            out_count.* = if (out_rows == null) idx else @min(idx, out_cap);
            return .ok;
        }

        return .read_row_fail;
    }
}

/// INSERT one fifo_snapshot row.
/// - `acq_commission_eur`: NaN or < 0 => bind NULL so DB default 0.0 can apply.
pub fn sqlite_insert_fifo_snapshot(handle: DbHandle, row: *const Row) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const broker = cstr_required(row.broker[0..]) orelse return .invalid_argument;
    const ticker = cstr_required(row.ticker[0..]) orelse return .invalid_argument;
    const acq_dt = cstr_required(row.acq_datetime[0..]) orelse return .invalid_argument;
    const country = cstr_required(row.country[0..]) orelse return .invalid_argument;

    const db: *c.sqlite3 = @ptrFromInt(handle);

    const sql: [:0]const u8 =
        \\INSERT INTO fifo_snapshot
        \\(broker, tax_year, ticker, acq_trade_id, acq_datetime,
        \\ qty_remaining, cost_per_share_eur, acq_commission_eur, country)
        \\VALUES
        \\(?1, ?2, ?3, ?4, ?5, ?6, ?7, COALESCE(?8, 0.0), ?9);
    ;

    var stmt: ?*c.sqlite3_stmt = null;
    const prep = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    if (prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    var ec: helper.ErrorCode = .ok;

    ec = bind_text(stmt.?, 1, broker);
    if (ec != .ok) return ec;
    ec = bind_u32(stmt.?, 2, row.tax_year);
    if (ec != .ok) return ec;
    ec = bind_text(stmt.?, 3, ticker);
    if (ec != .ok) return ec;
    ec = bind_u32(stmt.?, 4, row.acq_trade_id);
    if (ec != .ok) return ec;
    ec = bind_text(stmt.?, 5, acq_dt);
    if (ec != .ok) return ec;

    ec = bind_f64(stmt.?, 6, row.qty_remaining);
    if (ec != .ok) return ec;
    ec = bind_f64(stmt.?, 7, row.cost_per_share_eur);
    if (ec != .ok) return ec;
    ec = bind_nullable_f64_default0(stmt.?, 8, row.acq_commission_eur);
    if (ec != .ok) return ec;

    ec = bind_text(stmt.?, 9, country);
    if (ec != .ok) return ec;

    const step_rc = c.sqlite3_step(stmt.?);
    if (step_rc != c.SQLITE_DONE) return .execution_fail;

    return .ok;
}

pub fn sqlite_read_fifo_snapshot_all(
    handle: DbHandle,
    out_rows: ?[*]Row,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    out_count.* = 0;

    const db: *c.sqlite3 = @ptrFromInt(handle);

    const sql: [:0]const u8 =
        \\SELECT lot_id, broker, tax_year, ticker, acq_trade_id, acq_datetime,
        \\       qty_remaining, cost_per_share_eur, acq_commission_eur, country
        \\FROM fifo_snapshot
        \\ORDER BY broker, tax_year, ticker, acq_datetime ASC, lot_id;
    ;

    var stmt: ?*c.sqlite3_stmt = null;
    const prep = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    if (prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    return read_loop(stmt.?, out_rows, out_cap, out_count);
}

pub fn sqlite_read_fifo_snapshot_by_tax_year(
    handle: DbHandle,
    tax_year: u32,
    out_rows: ?[*]Row,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    out_count.* = 0;

    const db: *c.sqlite3 = @ptrFromInt(handle);

    const sql: [:0]const u8 =
        \\SELECT lot_id, broker, tax_year, ticker, acq_trade_id, acq_datetime,
        \\       qty_remaining, cost_per_share_eur, acq_commission_eur, country
        \\FROM fifo_snapshot
        \\WHERE tax_year <= ?1
        \\ORDER BY broker, ticker, acq_datetime ASC, lot_id;
    ;

    var stmt: ?*c.sqlite3_stmt = null;
    const prep = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    if (prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const ec = bind_u32(stmt.?, 1, tax_year);
    if (ec != .ok) return ec;

    return read_loop(stmt.?, out_rows, out_cap, out_count);
}

pub fn sqlite_read_fifo_snapshot_by_ticker_per_year(
    handle: DbHandle,
    tax_year: u32,
    ticker: [*:0]const u8,
    out_rows: ?[*]Row,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    out_count.* = 0;

    const db: *c.sqlite3 = @ptrFromInt(handle);

    const sql: [:0]const u8 =
        \\SELECT lot_id, broker, tax_year, ticker, acq_trade_id, acq_datetime,
        \\       qty_remaining, cost_per_share_eur, acq_commission_eur, country
        \\FROM fifo_snapshot
        \\WHERE tax_year <= ?1 AND ticker = ?2
        \\ORDER BY broker, acq_datetime ASC, lot_id;
    ;

    var stmt: ?*c.sqlite3_stmt = null;
    const prep = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    if (prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    var ec = bind_u32(stmt.?, 1, tax_year);
    if (ec != .ok) return ec;

    ec = bind_text(stmt.?, 2, ticker);
    if (ec != .ok) return ec;

    return read_loop(stmt.?, out_rows, out_cap, out_count);
}

pub fn sqlite_read_fifo_snapshot_by_broker_per_year(
    handle: DbHandle,
    tax_year: u32,
    broker: [*:0]const u8,
    out_rows: ?[*]Row,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    out_count.* = 0;

    const db: *c.sqlite3 = @ptrFromInt(handle);

    const sql: [:0]const u8 =
        \\SELECT lot_id, broker, tax_year, ticker, acq_trade_id, acq_datetime,
        \\       qty_remaining, cost_per_share_eur, acq_commission_eur, country
        \\FROM fifo_snapshot
        \\WHERE tax_year <= ?1 AND broker = ?2
        \\ORDER BY ticker, acq_datetime ASC, lot_id;
    ;

    var stmt: ?*c.sqlite3_stmt = null;
    const prep = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    if (prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    var ec = bind_u32(stmt.?, 1, tax_year);
    if (ec != .ok) return ec;

    ec = bind_text(stmt.?, 2, broker);
    if (ec != .ok) return ec;

    return read_loop(stmt.?, out_rows, out_cap, out_count);
}

pub fn sqlite_read_fifo_snapshot_by_year_broker_ticker(
    handle: DbHandle,
    tax_year: u32,
    broker: [*:0]const u8,
    ticker: [*:0]const u8,
    out_rows: ?[*]Row,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    out_count.* = 0;

    const db: *c.sqlite3 = @ptrFromInt(handle);

    const sql: [:0]const u8 =
        \\SELECT lot_id, broker, tax_year, ticker, acq_trade_id, acq_datetime,
        \\       qty_remaining, cost_per_share_eur, acq_commission_eur, country
        \\FROM fifo_snapshot
        \\WHERE tax_year <= ?1 AND broker = ?2 AND ticker = ?3
        \\ORDER BY acq_datetime ASC, lot_id;
    ;

    var stmt: ?*c.sqlite3_stmt = null;
    const prep = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    if (prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    var ec = bind_u32(stmt.?, 1, tax_year);
    if (ec != .ok) return ec;

    ec = bind_text(stmt.?, 2, broker);
    if (ec != .ok) return ec;

    ec = bind_text(stmt.?, 3, ticker);
    if (ec != .ok) return ec;

    return read_loop(stmt.?, out_rows, out_cap, out_count);
}

/// DELETE one fifo_snapshot row by lot_id.
/// Returns:
/// - .ok if a row was deleted
/// - .execution_fail if no row matched lot_id
/// - .preparation_fail / binder errors for sqlite failures
pub fn sqlite_delete_fifo_snapshot_by_lot_id(handle: DbHandle, lot_id: u32) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (lot_id == 0) return .invalid_argument;

    const db: *c.sqlite3 = @ptrFromInt(handle);

    const sql: [:0]const u8 =
        \\DELETE FROM fifo_snapshot
        \\WHERE lot_id = ?1;
    ;

    var stmt: ?*c.sqlite3_stmt = null;
    const prep = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    if (prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const ec = bind_u32(stmt.?, 1, lot_id);
    if (ec != .ok) return ec;

    const step_rc = c.sqlite3_step(stmt.?);
    if (step_rc != c.SQLITE_DONE) return .execution_fail;

    // Ensure something was actually deleted
    if (c.sqlite3_changes(db) == 0) return .execution_fail;

    return .ok;
}

/// UPDATE fifo_snapshot.qty_remaining by lot_id.
/// Returns:
/// - .ok if one or more rows updated (should be exactly 1)
/// - .execution_fail if lot_id not found
pub fn sqlite_update_fifo_snapshot_qty_remaining_by_lot_id(
    handle: DbHandle,
    lot_id: u32,
    qty_remaining: f64,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (lot_id == 0) return .invalid_argument;

    const db: *c.sqlite3 = @ptrFromInt(handle);

    const sql: [:0]const u8 =
        \\UPDATE fifo_snapshot
        \\SET qty_remaining = ?1
        \\WHERE lot_id = ?2;
    ;

    var stmt: ?*c.sqlite3_stmt = null;
    const prep = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    if (prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    var ec = bind_f64(stmt.?, 1, qty_remaining);
    if (ec != .ok) return ec;

    ec = bind_u32(stmt.?, 2, lot_id);
    if (ec != .ok) return ec;

    const step_rc = c.sqlite3_step(stmt.?);
    if (step_rc != c.SQLITE_DONE) return .execution_fail;

    if (c.sqlite3_changes(db) == 0) return .execution_fail;

    return .ok;
}
