const std = @import("std");
const testopts = @import("testopts");

// Import your public C-ABI surface (so we test what C callers use).
const api = @import("api");

// Reuse sqlite3 cImport already defined in sqliteConnector.zig.
const sqlite = api.sqlite;
const c = sqlite.c;

const helper = api.helper;
const DbHandle = helper.DbHandle;

// zp_trade is the unified struct you want to use everywhere.
const Trade = api.trade.zp_trade;

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

fn zbufToSlice(buf: []const u8) []const u8 {
    const end = std.mem.indexOfScalar(u8, buf, 0) orelse buf.len;
    return buf[0..end];
}

fn expectTradeText(buf: []const u8, expected: []const u8) !void {
    try std.testing.expectEqualStrings(expected, zbufToSlice(buf));
}

fn seedTrades(handle: DbHandle) !void {
    // Insert 4 trades with known brokers + years.
    // Fresh DB => ids will be 1..N in insertion order.

    const dt_2024_a: [:0]const u8 = "2024-01-15 10:00";
    const dt_2025_a: [:0]const u8 = "2025-02-10 09:00";
    const dt_2025_b: [:0]const u8 = "2025-06-11 14:30";
    const dt_2025_c: [:0]const u8 = "2025-12-20 18:45";

    const broker_ikbr: [:0]const u8 = "IKBR";
    const broker_rev: [:0]const u8 = "REVOLUT";

    const type_buy: [:0]const u8 = "BUY";
    const type_sell: [:0]const u8 = "SELL";

    const country_us: [:0]const u8 = "US";
    const currency_usd: [:0]const u8 = "USD";

    // id=1 (2024, IKBR)
    try std.testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_insert_trade(
            handle,
            dt_2024_a.ptr,
            "GE".ptr,
            1.0,
            100.0,
            broker_ikbr.ptr,
            type_buy.ptr,
            0.5,
            country_us.ptr,
            currency_usd.ptr,
            1.0,
        ),
    );

    // id=2 (2025, IKBR)
    try std.testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_insert_trade(
            handle,
            dt_2025_a.ptr,
            "OSTK".ptr,
            2.0,
            50.0,
            broker_ikbr.ptr,
            type_buy.ptr,
            0.0,
            country_us.ptr,
            currency_usd.ptr,
            1.0,
        ),
    );

    // id=3 (2025, REVOLUT)
    try std.testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_insert_trade(
            handle,
            dt_2025_b.ptr,
            "AAPL".ptr,
            3.0,
            200.0,
            broker_rev.ptr,
            type_buy.ptr,
            1.0,
            country_us.ptr,
            currency_usd.ptr,
            1.0,
        ),
    );

    // id=4 (2025, REVOLUT)
    try std.testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_insert_trade(
            handle,
            dt_2025_c.ptr,
            "MSFT".ptr,
            4.0,
            300.0,
            broker_rev.ptr,
            type_sell.ptr,
            2.0,
            country_us.ptr,
            currency_usd.ptr,
            1.0,
        ),
    );
}

test "read_trades_by_year: returns only that year in correct order" {
    vprint("RUNNING: read_trades_by_year: returns only that year in correct order");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    try seedTrades(handle);

    var out: [8]Trade = undefined;
    var count: usize = 0;

    const rc = api.zp_sqlite_read_trades_by_year(handle, 2025, @ptrCast(&out), out.len, &count);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 3), count);

    // Expect chronological order: 2025-02-10, 2025-06-11, 2025-12-20
    try expectTradeText(out[0].trade_datetime[0..], "2025-02-10 09:00");
    try expectTradeText(out[1].trade_datetime[0..], "2025-06-11 14:30");
    try expectTradeText(out[2].trade_datetime[0..], "2025-12-20 18:45");

    // Spot-check brokers
    try expectTradeText(out[0].broker[0..], "IKBR");
    try expectTradeText(out[1].broker[0..], "REVOLUT");
    try expectTradeText(out[2].broker[0..], "REVOLUT");
}

test "read_trades_by_broker: returns only that broker" {
    vprint("RUNNING: read_trades_by_broker: returns only that broker");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    try seedTrades(handle);

    const broker_rev: [:0]const u8 = "REVOLUT";

    var out: [8]Trade = undefined;
    var count: usize = 0;

    const rc = api.zp_sqlite_read_trades_by_broker(handle, broker_rev.ptr, @ptrCast(&out), out.len, &count);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 2), count);

    // Both rows must be REVOLUT and in time order
    try expectTradeText(out[0].broker[0..], "REVOLUT");
    try expectTradeText(out[1].broker[0..], "REVOLUT");

    try expectTradeText(out[0].trade_datetime[0..], "2025-06-11 14:30");
    try expectTradeText(out[1].trade_datetime[0..], "2025-12-20 18:45");

    // Spot-check tickers and numerics
    try expectTradeText(out[0].ticker[0..], "AAPL");
    try std.testing.expectApproxEqAbs(@as(f64, 3.0), out[0].quantity, 1e-12);

    try expectTradeText(out[1].ticker[0..], "MSFT");
    try std.testing.expectApproxEqAbs(@as(f64, 4.0), out[1].quantity, 1e-12);
}

test "read_trades_by_year_and_broker: returns only that year+broker" {
    vprint("RUNNING: read_trades_by_year_and_broker: returns only that year+broker");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    try seedTrades(handle);

    const broker_ikbr: [:0]const u8 = "IKBR";

    var out: [8]Trade = undefined;
    var count: usize = 0;

    const rc = api.zp_sqlite_read_trades_by_year_and_broker(
        handle,
        2025,
        broker_ikbr.ptr,
        @ptrCast(&out),
        out.len,
        &count,
    );
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 1), count);

    try expectTradeText(out[0].broker[0..], "IKBR");
    try expectTradeText(out[0].trade_datetime[0..], "2025-02-10 09:00");
    try expectTradeText(out[0].ticker[0..], "OSTK");
    try std.testing.expectApproxEqAbs(@as(f64, 2.0), out[0].quantity, 1e-12);
}

test "read_trades_by_broker: broker with no rows returns count=0" {
    vprint("RUNNING: read_trades_by_broker: broker with no rows returns count=0");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    try seedTrades(handle);

    const broker_none: [:0]const u8 = "NOPE";

    var out: [4]Trade = undefined;
    var count: usize = 123; // ensure function overwrites it

    const rc = api.zp_sqlite_read_trades_by_broker(handle, broker_none.ptr, @ptrCast(&out), out.len, &count);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 0), count);
}
