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
                    return .ok; // truncated safely
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

// ------------------------------------------------------------
// Public API (internal Zig-callable)
// ------------------------------------------------------------

pub fn sqlite_count_dividends_all(handle: DbHandle, out_count: *usize) helper.ErrorCode {
    out_count.* = 0;
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    var stmt: ?*c.sqlite3_stmt = null;

    const prep_rc: c_int = c.sqlite3_prepare_v2(db_ptr, sql_count_all.ptr, -1, &stmt, null);
    if (prep_rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const step_rc: c_int = c.sqlite3_step(stmt.?);
    if (step_rc != c.SQLITE_ROW) return .read_row_fail;

    const n = c.sqlite3_column_int64(stmt.?, 0);
    out_count.* = @as(usize, @intCast(@max(@as(i64, 0), n)));

    return .ok;
}

/// Read all dividends.
/// - If out_rows == null OR out_cap == 0: count-only via stepping (out_count = total)
/// - Else: write up to out_cap entries, out_count = number written (safe truncation)
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

    const prep_rc: c_int = c.sqlite3_prepare_v2(db_ptr, sql_read_all.ptr, -1, &stmt, null);
    if (prep_rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    return read_dividends_loop(stmt.?, out_rows, out_cap, out_count);
}
