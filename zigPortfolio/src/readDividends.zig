const std = @import("std");
const helper = @import("helper.zig");
const schema = @import("schemaStructs.zig");

pub const c = @cImport({
    @cInclude("sqlite3.h");
});

const DbHandle = helper.DbHandle;

// ------------------------------------------------------------
// SQL
// ------------------------------------------------------------

const sql_count_all: [:0]const u8 =
    \\SELECT COUNT(*) FROM dividends;
;

const sql_read_all: [:0]const u8 =
    \\SELECT dividend_id, broker, dividend_dt, ticker, country,
    \\       per_share, total_amount, tax,
    \\       currency, conversion_rate_eur
    \\FROM dividends
    \\ORDER BY dividend_dt ASC, dividend_id ASC;
;

// Year range: [YYYY-01-01, (YYYY+1)-01-01)
const sql_count_by_year: [:0]const u8 =
    \\SELECT COUNT(*)
    \\FROM dividends
    \\WHERE dividend_dt >= ?1
    \\  AND dividend_dt <  ?2;
;

const sql_read_by_year: [:0]const u8 =
    \\SELECT dividend_id, broker, dividend_dt, ticker, country,
    \\       per_share, total_amount, tax,
    \\       currency, conversion_rate_eur
    \\FROM dividends
    \\WHERE dividend_dt >= ?1
    \\  AND dividend_dt <  ?2
    \\ORDER BY dividend_dt ASC, dividend_id ASC;
;

const sql_count_by_country: [:0]const u8 =
    \\SELECT COUNT(*)
    \\FROM dividends
    \\WHERE country = ?1;
;

const sql_read_by_country: [:0]const u8 =
    \\SELECT dividend_id, broker, dividend_dt, ticker, country,
    \\       per_share, total_amount, tax,
    \\       currency, conversion_rate_eur
    \\FROM dividends
    \\WHERE country = ?1
    \\ORDER BY dividend_dt ASC, dividend_id ASC;
;

const sql_count_by_year_and_country: [:0]const u8 =
    \\SELECT COUNT(*)
    \\FROM dividends
    \\WHERE dividend_dt >= ?1
    \\  AND dividend_dt <  ?2
    \\  AND country = ?3;
;

const sql_read_by_year_and_country: [:0]const u8 =
    \\SELECT dividend_id, broker, dividend_dt, ticker, country,
    \\       per_share, total_amount, tax,
    \\       currency, conversion_rate_eur
    \\FROM dividends
    \\WHERE dividend_dt >= ?1
    \\  AND dividend_dt <  ?2
    \\  AND country = ?3
    \\ORDER BY dividend_dt ASC, dividend_id ASC;
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

fn stmt_to_dividend(stmt: *c.sqlite3_stmt, out: *schema.zp_dividend) void {
    const id_i64 = c.sqlite3_column_int64(stmt, 0);
    out.dividend_id = @as(u32, @intCast(@max(@as(i64, 0), @as(i64, @intCast(id_i64)))));

    copy_col_text_into(out.broker[0..], stmt, 1);
    copy_col_text_into(out.dividend_dt[0..], stmt, 2);
    copy_col_text_into(out.ticker[0..], stmt, 3);
    copy_col_text_into(out.country[0..], stmt, 4);

    out.per_share = c.sqlite3_column_double(stmt, 5);
    out.total_amount = c.sqlite3_column_double(stmt, 6);
    out.tax = c.sqlite3_column_double(stmt, 7);

    copy_col_text_into(out.currency[0..], stmt, 8);
    out.conversion_rate_eur = c.sqlite3_column_double(stmt, 9);
}

fn read_dividends_loop(
    stmt: *c.sqlite3_stmt,
    out_rows: ?[*]schema.zp_dividend,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    out_count.* = 0;

    var idx: usize = 0;
    while (true) {
        const rc: c_int = c.sqlite3_step(stmt);

        if (rc == c.SQLITE_ROW) {
            if (out_rows) |buf| {
                if (idx >= out_cap) {
                    out_count.* = idx;
                    return .ok; // safe truncation
                }
                stmt_to_dividend(stmt, &buf[idx]);
            }
            idx += 1;
            continue;
        }

        if (rc == c.SQLITE_DONE) {
            out_count.* = idx;
            return .ok;
        }

        return .read_row_fail;
    }
}

fn step_count(stmt: *c.sqlite3_stmt, out_count: *usize) helper.ErrorCode {
    out_count.* = 0;

    const rc: c_int = c.sqlite3_step(stmt);
    if (rc != c.SQLITE_ROW) return .read_row_fail;

    const n = c.sqlite3_column_int64(stmt, 0);
    out_count.* = @as(usize, @intCast(@max(@as(i64, 0), n)));
    return .ok;
}

fn bind_year_range(stmt: *c.sqlite3_stmt, idx_start: c_int, year: u32) helper.ErrorCode {
    // "YYYY-01-01" (10 chars) + '\0'
    var start_buf: [11]u8 = undefined;
    var end_buf: [11]u8 = undefined;

    _ = std.fmt.bufPrintZ(&start_buf, "{d:0>4}-01-01", .{year}) catch return .invalid_argument;
    _ = std.fmt.bufPrintZ(&end_buf, "{d:0>4}-01-01", .{year + 1}) catch return .invalid_argument;

    var rc: c_int = c.sqlite3_bind_text(
        stmt,
        idx_start,
        @ptrCast(start_buf[0..].ptr),
        -1,
        c.SQLITE_TRANSIENT,
    );
    if (rc != c.SQLITE_OK) return .preparation_fail;

    rc = c.sqlite3_bind_text(
        stmt,
        idx_start + 1,
        @ptrCast(end_buf[0..].ptr),
        -1,
        c.SQLITE_TRANSIENT,
    );
    if (rc != c.SQLITE_OK) return .preparation_fail;

    return .ok;
}

// ------------------------------------------------------------
// COUNT
// ------------------------------------------------------------

pub fn sqlite_count_dividends_all(handle: DbHandle, out_count: *usize) helper.ErrorCode {
    out_count.* = 0;
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_count_all.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    return step_count(stmt.?, out_count);
}

pub fn sqlite_count_dividends_by_year(handle: DbHandle, year: u32, out_count: *usize) helper.ErrorCode {
    out_count.* = 0;
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_count_by_year.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const ec = bind_year_range(stmt.?, 1, year);
    if (ec != .ok) return ec;

    return step_count(stmt.?, out_count);
}

pub fn sqlite_count_dividends_by_country(handle: DbHandle, country: [*:0]const u8, out_count: *usize) helper.ErrorCode {
    out_count.* = 0;
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_count_by_country.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const rc_bind: c_int = c.sqlite3_bind_text(stmt.?, 1, country, -1, c.SQLITE_TRANSIENT);
    if (rc_bind != c.SQLITE_OK) return .preparation_fail;

    return step_count(stmt.?, out_count);
}

pub fn sqlite_count_dividends_by_year_and_country(
    handle: DbHandle,
    year: u32,
    country: [*:0]const u8,
    out_count: *usize,
) helper.ErrorCode {
    out_count.* = 0;
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_count_by_year_and_country.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const ec = bind_year_range(stmt.?, 1, year);
    if (ec != .ok) return ec;

    const rc_bind: c_int = c.sqlite3_bind_text(stmt.?, 3, country, -1, c.SQLITE_TRANSIENT);
    if (rc_bind != c.SQLITE_OK) return .preparation_fail;

    return step_count(stmt.?, out_count);
}

// ------------------------------------------------------------
// READ
// ------------------------------------------------------------

pub fn sqlite_read_all_dividends(
    handle: DbHandle,
    out_rows: ?[*]schema.zp_dividend,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    out_count.* = 0;
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_all.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    return read_dividends_loop(stmt.?, out_rows, out_cap, out_count);
}

pub fn sqlite_read_dividends_by_year(
    handle: DbHandle,
    year: u32,
    out_rows: ?[*]schema.zp_dividend,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    out_count.* = 0;
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_by_year.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const ec = bind_year_range(stmt.?, 1, year);
    if (ec != .ok) return ec;

    return read_dividends_loop(stmt.?, out_rows, out_cap, out_count);
}

pub fn sqlite_read_dividends_by_country(
    handle: DbHandle,
    country: [*:0]const u8,
    out_rows: ?[*]schema.zp_dividend,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    out_count.* = 0;
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_by_country.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const rc_bind: c_int = c.sqlite3_bind_text(stmt.?, 1, country, -1, c.SQLITE_TRANSIENT);
    if (rc_bind != c.SQLITE_OK) return .preparation_fail;

    return read_dividends_loop(stmt.?, out_rows, out_cap, out_count);
}

pub fn sqlite_read_dividends_by_year_and_country(
    handle: DbHandle,
    year: u32,
    country: [*:0]const u8,
    out_rows: ?[*]schema.zp_dividend,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    out_count.* = 0;
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const rc_prep: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_by_year_and_country.ptr, -1, &stmt, null);
    if (rc_prep != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const ec = bind_year_range(stmt.?, 1, year);
    if (ec != .ok) return ec;

    const rc_bind: c_int = c.sqlite3_bind_text(stmt.?, 3, country, -1, c.SQLITE_TRANSIENT);
    if (rc_bind != c.SQLITE_OK) return .preparation_fail;

    return read_dividends_loop(stmt.?, out_rows, out_cap, out_count);
}
