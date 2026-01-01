const std = @import("std");
const testopts = @import("testopts");

const api = @import("api");
const sqlite = api.sqlite;
const c = sqlite.c;

pub const helper = api.helper;
pub const trade = api.trade;

pub inline fn vprint(comptime msg: []const u8) void {
    if (testopts.verbose_test_names) {
        std.debug.print("{s}\n", .{msg});
    }
}

pub const schema_sql: [:0]const u8 =
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

pub fn execSql(db: *c.sqlite3, sql: [:0]const u8) !void {
    var errmsg: [*c]u8 = null;
    const errmsg_ptr: [*c][*c]u8 = @ptrCast(&errmsg);

    const rc: c_int = c.sqlite3_exec(db, sql.ptr, null, null, errmsg_ptr);
    defer {
        if (errmsg != null) c.sqlite3_free(errmsg);
    }
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), rc);
}

pub fn openMemDb() !helper.DbHandle {
    const mem: [:0]const u8 = ":memory:";
    var handle: helper.DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);
    return handle;
}

pub fn setBufZ(buf: []u8, s: []const u8) void {
    @memset(buf, 0);
    if (buf.len == 0) return;
    const n = @min(s.len, buf.len - 1);
    std.mem.copyForwards(u8, buf[0..n], s[0..n]);
    buf[n] = 0;
}

pub fn countTrades(db: *c.sqlite3) !i64 {
    var stmt: ?*c.sqlite3_stmt = null;
    const sql: [:0]const u8 = "SELECT COUNT(*) FROM trades;";

    var rc: c_int = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), rc);
    defer _ = c.sqlite3_finalize(stmt.?);

    rc = c.sqlite3_step(stmt.?);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_ROW), rc);

    return @as(i64, c.sqlite3_column_int64(stmt.?, 0));
}
