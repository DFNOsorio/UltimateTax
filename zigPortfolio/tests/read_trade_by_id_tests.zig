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

fn expectTrade(out: *const api.zp_trade, exp: struct {
    broker: []const u8,
    dt: []const u8,
    trade_type: []const u8,
    ticker: []const u8,
    quantity: f64,
    pps: f64,
    commission: f64,
    country: []const u8,
    currency: []const u8,
    conv: f64,
}) !void {
    try std.testing.expectEqualStrings(exp.broker, zstr(out.broker[0..]));
    try std.testing.expectEqualStrings(exp.dt, zstr(out.trade_datetime[0..]));
    try std.testing.expectEqualStrings(exp.trade_type, zstr(out.trade_type[0..]));
    try std.testing.expectEqualStrings(exp.ticker, zstr(out.ticker[0..]));

    try std.testing.expectApproxEqAbs(exp.quantity, out.quantity, 1e-12);
    try std.testing.expectApproxEqAbs(exp.pps, out.price_per_share, 1e-12);
    try std.testing.expectApproxEqAbs(exp.commission, out.commission, 1e-12);

    try std.testing.expectEqualStrings(exp.country, zstr(out.country[0..]));
    try std.testing.expectEqualStrings(exp.currency, zstr(out.currency[0..]));
    try std.testing.expectApproxEqAbs(exp.conv, out.conversion_rate_eur, 1e-12);
}

test "read_trade_by_id: returns row for existing id" {
    vprint("RUNNING: read_trade_by_id: returns row for existing id");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    // Insert 1
    try std.testing.expectEqual(.ok, api.zp_sqlite_insert_trade(
        handle,
        "2025-01-02 10:00".ptr,
        "GE".ptr,
        10.0,
        121.50,
        "IKBR".ptr,
        "SELL".ptr,
        1.23,
        "US".ptr,
        "USD".ptr,
        1.0,
    ));
    const id1: u32 = @intCast(c.sqlite3_last_insert_rowid(db));

    // Insert 2
    try std.testing.expectEqual(.ok, api.zp_sqlite_insert_trade(
        handle,
        "2025-02-03 09:30".ptr,
        "OSTK".ptr,
        2.0,
        50.0,
        "XTB".ptr,
        "BUY".ptr,
        0.0,
        "US".ptr,
        "USD".ptr,
        1.0,
    ));
    _ = c.sqlite3_last_insert_rowid(db);

    var out: api.zp_trade = std.mem.zeroes(api.zp_trade);

    const rc = api.zp_sqlite_read_trade_by_id(handle, id1, &out);
    try std.testing.expectEqual(.ok, rc);

    try expectTrade(&out, .{
        .broker = "IKBR",
        .dt = "2025-01-02 10:00",
        .trade_type = "SELL",
        .ticker = "GE",
        .quantity = 10.0,
        .pps = 121.50,
        .commission = 1.23,
        .country = "US",
        .currency = "USD",
        .conv = 1.0,
    });
}

test "read_trade_by_id: returns execution_fail when id not found" {
    vprint("RUNNING: read_trade_by_id: returns execution_fail when id not found");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    var out: api.zp_trade = std.mem.zeroes(api.zp_trade);
    const rc = api.zp_sqlite_read_trade_by_id(handle, 999999, &out);
    try std.testing.expectEqual(.execution_fail, rc);
}
