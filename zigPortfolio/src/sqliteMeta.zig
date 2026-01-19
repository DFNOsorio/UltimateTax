const std = @import("std");
const helper = @import("helper.zig");
const trade = @import("schemaStructs.zig");
const sqlite = @import("sqliteConnector.zig");

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

pub const zp_table = enum(u32) {
    trades = 0,
    fifo_snapshot = 1,
    fifo_realized = 2,
};

pub fn sqlite_count_rows(
    db: DbHandle,
    table: zp_table,
    year: ?u32,
    broker: ?[:0]const u8,
    ticker: ?[:0]const u8,
    out_count: *usize,
) helper.ErrorCode {
    out_count.* = 0;

    // Use a bigger fixed buffer to be safe, but the key is: build in one pass
    // (no intermediate allocPrint slices that permanently consume the FBA).
    var buf: [512]u8 = undefined;
    var fba = std.heap.FixedBufferAllocator.init(&buf);
    const allocator = fba.allocator();

    const base_table: []const u8 = switch (table) {
        .trades => "trades",
        .fifo_snapshot => "fifo_snapshot",
        .fifo_realized => "fifo_realized",
    };

    var q = std.ArrayList(u8){};
    defer q.deinit(allocator);

    const w = q.writer(allocator);

    // SELECT COUNT(*) FROM <table>
    w.print("SELECT COUNT(*) FROM {s}", .{base_table}) catch return .preparation_fail;

    // Build WHERE in a single pass
    var has_where = false;

    // year filter
    if (year) |y| {
        w.print(" WHERE ", .{}) catch return .preparation_fail;
        has_where = true;

        switch (table) {
            .trades => w.print("substr(trade_datetime,1,4) = '{d}'", .{y}) catch return .preparation_fail,
            .fifo_snapshot => w.print("tax_year <= {d}", .{y}) catch return .preparation_fail,
            .fifo_realized => w.print("tax_year = {d}", .{y}) catch return .preparation_fail,
        }
    }

    // broker filter
    if (broker) |b0| {
        const b = std.mem.sliceTo(b0, 0); // drop sentinel for formatting
        if (!has_where) {
            w.print(" WHERE ", .{}) catch return .preparation_fail;
            has_where = true;
        } else {
            w.print(" AND ", .{}) catch return .preparation_fail;
        }
        w.print("broker = '{s}'", .{b}) catch return .preparation_fail;
    }

    // ticker filter
    if (ticker) |t0| {
        const t = std.mem.sliceTo(t0, 0); // drop sentinel for formatting
        if (!has_where) {
            w.print(" WHERE ", .{}) catch return .preparation_fail;
            has_where = true;
        } else {
            w.print(" AND ", .{}) catch return .preparation_fail;
        }
        w.print("ticker = '{s}'", .{t}) catch return .preparation_fail;
    }

    // sqlite3_prepare_v2 with -1 expects NUL-terminated SQL
    q.append(allocator, 0) catch return .preparation_fail;

    const db_ptr: *c.sqlite3 = @ptrFromInt(db);

    var stmt: ?*c.sqlite3_stmt = null;
    const prep_rc = c.sqlite3_prepare_v2(
        db_ptr,
        @as([*:0]const u8, @ptrCast(q.items.ptr)),
        -1,
        &stmt,
        null,
    );
    if (prep_rc != c.SQLITE_OK or stmt == null)
        return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const step_rc = c.sqlite3_step(stmt.?);
    if (step_rc != c.SQLITE_ROW)
        return .read_row_fail;

    const n = c.sqlite3_column_int64(stmt.?, 0);
    out_count.* = @as(usize, @intCast(@max(@as(i64, 0), n)));

    return .ok;
}

// ------------------------------------------------------------
// COUNT(*) helpers for trades by year + side
// ------------------------------------------------------------

fn sqlite_count_trades_by_year_and_side(
    db: DbHandle,
    year: u32,
    side_upper: []const u8, // "BUY" or "SELL"
    out_count: *usize,
) helper.ErrorCode {
    out_count.* = 0;
    if (db == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (year == 0) return .invalid_argument;

    // Keep the pattern aligned with sqlite_count_rows: fixed buffer + single-pass build.
    var buf: [256]u8 = undefined;
    var fba = std.heap.FixedBufferAllocator.init(&buf);
    const allocator = fba.allocator();

    var q = std.ArrayList(u8){};
    defer q.deinit(allocator);

    const w = q.writer(allocator);
    // NOTE: schema constrains type to BUY/SELL, but we still use upper() for safety.
    w.print(
        "SELECT COUNT(*) FROM trades WHERE substr(trade_datetime,1,4) = '{d}' AND upper(type) = '{s}'",
        .{ year, side_upper },
    ) catch return .preparation_fail;

    // sqlite3_prepare_v2 with -1 expects NUL-terminated SQL
    q.append(allocator, 0) catch return .preparation_fail;

    const db_ptr: *c.sqlite3 = @ptrFromInt(db);

    var stmt: ?*c.sqlite3_stmt = null;
    const prep_rc = c.sqlite3_prepare_v2(
        db_ptr,
        @as([*:0]const u8, @ptrCast(q.items.ptr)),
        -1,
        &stmt,
        null,
    );
    if (prep_rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const step_rc = c.sqlite3_step(stmt.?);
    if (step_rc != c.SQLITE_ROW) return .read_row_fail;

    const n = c.sqlite3_column_int64(stmt.?, 0);
    out_count.* = @as(usize, @intCast(@max(@as(i64, 0), n)));
    return .ok;
}

pub fn sqlite_count_buy_trades_by_year(
    db: DbHandle,
    year: u32,
    out_count: *usize,
) helper.ErrorCode {
    return sqlite_count_trades_by_year_and_side(db, year, "BUY", out_count);
}

pub fn sqlite_count_sell_trades_by_year(
    db: DbHandle,
    year: u32,
    out_count: *usize,
) helper.ErrorCode {
    return sqlite_count_trades_by_year_and_side(db, year, "SELL", out_count);
}

// Uses idx_fifo_snapshot_acq_trade
const sql_snapshot_exists_by_acq_trade: [:0]const u8 =
    "SELECT 1 FROM fifo_snapshot WHERE acq_trade_id = ?1 LIMIT 1;";

// Uses idx_fifo_realized_sell_trade
const sql_realized_exists_by_sell_trade: [:0]const u8 =
    "SELECT 1 FROM fifo_realized WHERE sell_trade_id = ?1 LIMIT 1;";

pub fn sqlite_fifo_snapshot_exists_by_acq_trade_id(
    handle: DbHandle,
    acq_trade_id: u32,
    out_exists: *bool,
) helper.ErrorCode {
    out_exists.* = false;
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (acq_trade_id == 0) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);

    var stmt: ?*c.sqlite3_stmt = null;
    const prep_rc = c.sqlite3_prepare_v2(db_ptr, sql_snapshot_exists_by_acq_trade.ptr, -1, &stmt, null);
    if (prep_rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const bind_rc = c.sqlite3_bind_int64(stmt.?, 1, @as(c.sqlite3_int64, @intCast(acq_trade_id)));
    if (bind_rc != c.SQLITE_OK) return .preparation_fail;

    const step_rc = c.sqlite3_step(stmt.?);
    if (step_rc == c.SQLITE_ROW) {
        out_exists.* = true;
        return .ok;
    }
    if (step_rc == c.SQLITE_DONE) {
        out_exists.* = false;
        return .ok;
    }

    return .read_row_fail;
}

pub fn sqlite_fifo_realized_exists_by_sell_trade_id(
    handle: DbHandle,
    sell_trade_id: u32,
    out_exists: *bool,
) helper.ErrorCode {
    out_exists.* = false;
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (sell_trade_id == 0) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);

    var stmt: ?*c.sqlite3_stmt = null;
    const prep_rc = c.sqlite3_prepare_v2(db_ptr, sql_realized_exists_by_sell_trade.ptr, -1, &stmt, null);
    if (prep_rc != c.SQLITE_OK or stmt == null) return .preparation_fail;
    defer _ = c.sqlite3_finalize(stmt.?);

    const bind_rc = c.sqlite3_bind_int64(stmt.?, 1, @as(c.sqlite3_int64, @intCast(sell_trade_id)));
    if (bind_rc != c.SQLITE_OK) return .preparation_fail;

    const step_rc = c.sqlite3_step(stmt.?);
    if (step_rc == c.SQLITE_ROW) {
        out_exists.* = true;
        return .ok;
    }
    if (step_rc == c.SQLITE_DONE) {
        out_exists.* = false;
        return .ok;
    }

    return .read_row_fail;
}
