const std = @import("std");
const testopts = @import("testopts");

// Test the public C-ABI surface (what C callers use).
const api = @import("api");

// Reuse sqlite3 cImport from sqliteConnector.
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

fn spanOpt(p: ?[*:0]const u8) []const u8 {
    return if (p) |q| std.mem.span(q) else "";
}

test "read_trade_by_id: returns trade when found" {
    vprint("RUNNING: read_trade_by_id: returns trade when found");

    const mem: [:0]const u8 = ":memory:";
    const dt: [:0]const u8 = "2025-12-27 10:00";
    const ticker_ge: [:0]const u8 = "GE";
    const broker_ikbr: [:0]const u8 = "IKBR";
    const type_sell: [:0]const u8 = "SELL";
    const country_us: [:0]const u8 = "US";
    const currency_usd: [:0]const u8 = "USD";

    var handle: DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);

    // Create schema via SQL
    try execSql(db, schema_sql);

    // Insert seed data into that schema using your API
    const ins_rc = api.zp_sqlite_insert_trade(
        handle,
        dt.ptr,
        ticker_ge.ptr,
        10.0,
        121.50,
        broker_ikbr.ptr,
        type_sell.ptr,
        1.23,
        country_us.ptr,
        currency_usd.ptr,
        1.0,
    );
    try std.testing.expectEqual(helper.ErrorCode.ok, ins_rc);

    const id_i64: i64 = c.sqlite3_last_insert_rowid(db);
    try std.testing.expect(id_i64 > 0);

    var out_trade: api.zp_trade = std.mem.zeroes(api.zp_trade);
    defer api.zp_trade_free(&out_trade);

    const rc = api.zp_sqlite_read_trade_by_id(handle, @intCast(id_i64), &out_trade);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);

    try std.testing.expectEqualStrings("IKBR", spanOpt(out_trade.broker));
    try std.testing.expectEqualStrings("2025-12-27 10:00", spanOpt(out_trade.trade_datetime));
    try std.testing.expectEqualStrings("SELL", spanOpt(out_trade.type));
    try std.testing.expectEqualStrings("GE", spanOpt(out_trade.ticker));

    try std.testing.expectApproxEqAbs(@as(f64, 10.0), out_trade.quantity, 1e-12);
    try std.testing.expectApproxEqAbs(@as(f64, 121.50), out_trade.price_per_share, 1e-12);
    try std.testing.expectApproxEqAbs(@as(f64, 1.23), out_trade.commission, 1e-12);

    try std.testing.expectEqualStrings("US", spanOpt(out_trade.country));
    try std.testing.expectEqualStrings("USD", spanOpt(out_trade.currency));
    try std.testing.expectApproxEqAbs(@as(f64, 1.0), out_trade.conversion_rate_eur, 1e-12);
}

test "read_trade_by_id: ok + null fields when not found" {
    vprint("RUNNING: read_trade_by_id: ok + null fields when not found");

    const mem: [:0]const u8 = ":memory:";

    var handle: DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);

    // Create schema via SQL
    try execSql(db, schema_sql);

    var out_trade: api.zp_trade = std.mem.zeroes(api.zp_trade);
    defer api.zp_trade_free(&out_trade);

    const rc = api.zp_sqlite_read_trade_by_id(handle, 999999, &out_trade);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);

    // Not found => keep pointers null (the behavior we want for a C ABI out struct)
    try std.testing.expect(out_trade.trade_datetime == null);
    try std.testing.expect(out_trade.ticker == null);
    try std.testing.expect(out_trade.broker == null);
    try std.testing.expect(out_trade.type == null);
    try std.testing.expect(out_trade.country == null);
    try std.testing.expect(out_trade.currency == null);
}

test "read_trade_by_id: invalid_argument when out_trade is NULL" {
    vprint("RUNNING: read_trade_by_id: invalid_argument when out_trade is NULL");

    const mem: [:0]const u8 = ":memory:";

    var handle: DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    const rc = api.zp_sqlite_read_trade_by_id(handle, 1, null);
    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, rc);
}

test "read_trade_by_id: invalid_argument when handle is invalid" {
    vprint("RUNNING: read_trade_by_id: invalid_argument when handle is invalid");

    var out_trade: api.zp_trade = std.mem.zeroes(api.zp_trade);
    defer api.zp_trade_free(&out_trade);

    const rc = api.zp_sqlite_read_trade_by_id(helper.INVALID_DB_HANDLE, 1, &out_trade);
    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, rc);
}
