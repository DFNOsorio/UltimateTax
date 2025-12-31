const std = @import("std");
const testopts = @import("testopts");
const api = @import("api");

const sqlite = api.sqlite;
const c = sqlite.c;

const helper = api.helper;
const DbHandle = helper.DbHandle;

inline fn vprint(comptime msg: []const u8) void {
    if (testopts.verbose_test_names) std.debug.print("{s}\n", .{msg});
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

fn execSql(db: *c.sqlite3, sql: [:0]const u8) !void {
    var errmsg: [*c]u8 = null;
    const errmsg_ptr: [*c][*c]u8 = @ptrCast(&errmsg);
    const rc: c_int = c.sqlite3_exec(db, sql.ptr, null, null, errmsg_ptr);
    defer if (errmsg != null) c.sqlite3_free(errmsg);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), rc);
}

fn zstr(buf: []const u8) []const u8 {
    const n = std.mem.indexOfScalar(u8, buf, 0) orelse buf.len;
    return buf[0..n];
}

test "read_trades_by_broker: returns only rows for requested broker" {
    vprint("RUNNING: read_trades_by_broker: returns only rows for requested broker");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    // IKBR
    try std.testing.expectEqual(.ok, api.zp_sqlite_insert_trade(handle, "2025-01-01 10:00".ptr, "GE".ptr, 1, 100, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1));
    // XTB
    try std.testing.expectEqual(.ok, api.zp_sqlite_insert_trade(handle, "2025-01-02 10:00".ptr, "AAPL".ptr, 1, 200, "XTB".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1));
    // IKBR again
    try std.testing.expectEqual(.ok, api.zp_sqlite_insert_trade(handle, "2025-01-03 10:00".ptr, "OSTK".ptr, 2, 50, "IKBR".ptr, "SELL".ptr, 1.2, "US".ptr, "USD".ptr, 1));

    var out: [8]api.zp_trade = [_]api.zp_trade{std.mem.zeroes(api.zp_trade)} ** 8;
    var count: usize = 0;

    const rc = api.zp_sqlite_read_trades_by_broker(handle, "IKBR".ptr, out[0..].ptr, out.len, &count);
    try std.testing.expectEqual(.ok, rc);
    try std.testing.expectEqual(@as(usize, 2), count);

    try std.testing.expectEqualStrings("IKBR", zstr(out[0].broker[0..]));
    try std.testing.expectEqualStrings("IKBR", zstr(out[1].broker[0..]));
    try std.testing.expectEqualStrings("GE", zstr(out[0].ticker[0..]));
    try std.testing.expectEqualStrings("OSTK", zstr(out[1].ticker[0..]));
}
