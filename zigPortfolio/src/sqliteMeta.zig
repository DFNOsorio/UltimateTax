const std = @import("std");
const helper = @import("helper.zig");
const trade = @import("trade.zig");

pub const c = @cImport({
    @cInclude("sqlite3.h");
});

const DbHandle = helper.DbHandle;

const sql_count_unique_brokers: [:0]const u8 =
    "SELECT COUNT(DISTINCT broker) FROM trades;";

const sql_select_unique_brokers: [:0]const u8 =
    "SELECT DISTINCT broker FROM trades ORDER BY broker ASC;";

const sql_count_unique_years: [:0]const u8 =
    "SELECT COUNT(DISTINCT CAST(substr(trade_datetime,1,4) AS INTEGER)) FROM trades;";

const sql_select_unique_years: [:0]const u8 =
    "SELECT DISTINCT CAST(substr(trade_datetime,1,4) AS INTEGER) AS y FROM trades ORDER BY y ASC;";

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

/// Pattern:
/// - If out_brokers == null OR out_cap == 0 => count-only, returns total in out_count.
/// - Else writes up to out_cap entries, sets out_count = number written (safe for iteration).
pub fn sqlite_get_unique_brokers(
    handle: DbHandle,
    out_brokers: ?[*]trade.zp_broker_name,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    out_count.* = 0;
    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);

    // count-only mode
    if (out_brokers == null or out_cap == 0) {
        var stmt: ?*c.sqlite3_stmt = null;
        const prep_rc = c.sqlite3_prepare_v2(db_ptr, sql_count_unique_brokers.ptr, -1, &stmt, null);
        if (prep_rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
        defer _ = c.sqlite3_finalize(stmt.?);

        const step_rc = c.sqlite3_step(stmt.?);
        if (step_rc != c.SQLITE_ROW) return .read_row_fail;

        const n = c.sqlite3_column_int64(stmt.?, 0);
        out_count.* = @as(usize, @intCast(@max(@as(i64, 0), @as(i64, @intCast(n)))));
        return .ok;
    }

    // fill mode
    var stmt: ?*c.sqlite3_stmt = null;
    const prep_rc = c.sqlite3_prepare_v2(db_ptr, sql_select_unique_brokers.ptr, -1, &stmt, null);
    if (prep_rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    var idx: usize = 0;
    while (idx < out_cap) {
        const rc = c.sqlite3_step(stmt.?);
        if (rc == c.SQLITE_ROW) {
            copy_col_text_into(out_brokers.?[idx].name[0..], stmt.?, 0);
            idx += 1;
            continue;
        }
        if (rc == c.SQLITE_DONE) break;
        return .read_row_fail;
    }

    out_count.* = idx;
    return .ok;
}

/// Pattern:
/// - If out_years == null OR out_cap == 0 => count-only total.
/// - Else writes up to out_cap entries, sets out_count = number written.
pub fn sqlite_get_unique_years(
    handle: DbHandle,
    out_years: ?[*]u32,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    out_count.* = 0;
    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);

    // count-only mode
    if (out_years == null or out_cap == 0) {
        var stmt: ?*c.sqlite3_stmt = null;
        const prep_rc = c.sqlite3_prepare_v2(db_ptr, sql_count_unique_years.ptr, -1, &stmt, null);
        if (prep_rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
        defer _ = c.sqlite3_finalize(stmt.?);

        const step_rc = c.sqlite3_step(stmt.?);
        if (step_rc != c.SQLITE_ROW) return .read_row_fail;

        const n = c.sqlite3_column_int64(stmt.?, 0);
        out_count.* = @as(usize, @intCast(@max(@as(i64, 0), @as(i64, @intCast(n)))));
        return .ok;
    }

    // fill mode
    var stmt: ?*c.sqlite3_stmt = null;
    const prep_rc = c.sqlite3_prepare_v2(db_ptr, sql_select_unique_years.ptr, -1, &stmt, null);
    if (prep_rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    var idx: usize = 0;
    while (idx < out_cap) {
        const rc = c.sqlite3_step(stmt.?);
        if (rc == c.SQLITE_ROW) {
            const y = c.sqlite3_column_int(stmt.?, 0);
            out_years.?[idx] = @as(u32, @intCast(@max(y, 0)));
            idx += 1;
            continue;
        }
        if (rc == c.SQLITE_DONE) break;
        return .read_row_fail;
    }

    out_count.* = idx;
    return .ok;
}
