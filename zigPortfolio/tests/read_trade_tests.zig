const std = @import("std");
const testopts = @import("testopts");

const api = @import("api");
const sqlite = api.sqlite;
const c = sqlite.c;

const helper = api.helper;
const DbHandle = helper.DbHandle;

inline fn vprint(comptime msg: []const u8) void {
    if (testopts.verbose_test_names) {
        std.debug.print("{s}\n", .{msg});
    }
}

const schema_sql: [:0]const u8 =
    \\PRAGMA foreign_keys = ON;
    \\DROP TABLE IF EXISTS trades;
    \\CREATE TABLE trades (
    \\    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    \\    broker              TEXT NOT NULL DEFAULT 'IKBR',
    \\    trade_datetime      TEXT NOT NULL,
    \\    type                TEXT NOT NULL CHECK (type IN ('BUY', 'SELL')) DEFAULT 'BUY',
    \\    ticker              TEXT NOT NULL,
    \\    quantity            REAL NOT NULL,
    \\    price_per_share     REAL NOT NULL,
    \\    commission          REAL NOT NULL DEFAULT 0.0,
    \\    country             TEXT NOT NULL DEFAULT 'US',
    \\    currency            TEXT NOT NULL DEFAULT 'USD',
    \\    conversion_rate_eur REAL NOT NULL DEFAULT 1.0
    \\);
    \\CREATE INDEX idx_trades_datetime ON trades(trade_datetime);
    \\CREATE INDEX idx_trades_ticker_datetime ON trades(ticker, trade_datetime);
;

// sqlite3_exec signature in Zig 0.15.2 wants errmsg: [*c][*c]u8
fn execSql(db: *c.sqlite3, sql: [:0]const u8) !void {
    var errmsg: [*c]u8 = null;
    const errmsg_ptr: [*c][*c]u8 = @ptrCast(&errmsg);

    const rc: c_int = c.sqlite3_exec(db, sql.ptr, null, null, errmsg_ptr);

    defer {
        if (errmsg != null) c.sqlite3_free(errmsg);
    }

    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), rc);
}

fn insertSeedTrades(handle: DbHandle) helper.ErrorCode {
    // We rely on sqliteConnector’s COALESCE defaults + sentinel normalization.
    // Insert 3 rows:
    //  id=1: 2024
    //  id=2: 2025
    //  id=3: 2025
    const rc1 = api.zp_sqlite_insert_trade(
        handle,
        "2024-06-01 10:00",
        "GE",
        1.0,
        100.0,
        "IKBR",
        "BUY",
        0.0,
        "US",
        "USD",
        1.0,
    );
    if (rc1 != .ok) return rc1;

    const rc2 = api.zp_sqlite_insert_trade(
        handle,
        "2025-01-02 09:30",
        "AAPL",
        2.0,
        200.0,
        null,
        null,
        std.math.nan(f64),
        null,
        null,
        0.0,
    );
    if (rc2 != .ok) return rc2;

    const rc3 = api.zp_sqlite_insert_trade(
        handle,
        "2025-12-31 15:45",
        "MSFT",
        3.0,
        300.0,
        "IKBR",
        "SELL",
        1.0,
        "US",
        "USD",
        1.0,
    );
    return rc3;
}

fn cstrSlice(buf: []const u8) []const u8 {
    const n = std.mem.indexOfScalar(u8, buf, 0) orelse buf.len;
    return buf[0..n];
}

test "read_trade_by_id: reads expected row into zp_trade" {
    vprint("RUNNING: read_trade_by_id: reads expected row into zp_trade");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);
    try std.testing.expectEqual(helper.ErrorCode.ok, insertSeedTrades(handle));

    var out: api.zp_trade = std.mem.zeroes(api.zp_trade);

    const rc = api.zp_sqlite_read_trade_by_id(handle, 2, &out);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);

    try std.testing.expectEqualStrings("2025-01-02 09:30", cstrSlice(&out.trade_datetime));
    try std.testing.expectEqualStrings("AAPL", cstrSlice(&out.ticker));

    // Defaults were applied at insert time
    try std.testing.expectEqualStrings("IKBR", cstrSlice(&out.broker));
    try std.testing.expectEqualStrings("BUY", cstrSlice(&out.trade_type));
}

test "read_trade_by_id: execution_fail when id not found" {
    vprint("RUNNING: read_trade_by_id: execution_fail when id not found");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);
    try std.testing.expectEqual(helper.ErrorCode.ok, insertSeedTrades(handle));

    var out: api.zp_trade = std.mem.zeroes(api.zp_trade);
    const rc = api.zp_sqlite_read_trade_by_id(handle, 999, &out);
    try std.testing.expectEqual(helper.ErrorCode.execution_fail, rc);
}

test "read_trades_by_year: returns only trades for requested year" {
    vprint("RUNNING: read_trades_by_year: returns only trades for requested year");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);
    try std.testing.expectEqual(helper.ErrorCode.ok, insertSeedTrades(handle));

    var out: [8]api.zp_trade = undefined;
    @memset(&out, std.mem.zeroes(api.zp_trade));

    var out_count: usize = 0;

    const rc = api.zp_sqlite_read_trades_by_year(handle, 2025, &out, out.len, &out_count);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 2), out_count);

    try std.testing.expectEqualStrings("AAPL", cstrSlice(&out[0].ticker));
    try std.testing.expectEqualStrings("MSFT", cstrSlice(&out[1].ticker));
}

test "read_trades_by_year: truncates safely to out_cap" {
    vprint("RUNNING: read_trades_by_year: truncates safely to out_cap");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);
    try std.testing.expectEqual(helper.ErrorCode.ok, insertSeedTrades(handle));

    var out: [1]api.zp_trade = undefined;
    @memset(&out, std.mem.zeroes(api.zp_trade));

    var out_count: usize = 0;

    const rc = api.zp_sqlite_read_trades_by_year(handle, 2025, &out, out.len, &out_count);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 1), out_count);
}
