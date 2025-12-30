const std = @import("std");
const testopts = @import("testopts");

// Import your public C-ABI surface (so we test what C callers use).
const api = @import("api");

// Reuse sqlite3 cImport already defined in sqliteConnector.zig.
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

fn colText(stmt: *c.sqlite3_stmt, col: c_int) []const u8 {
    const p = c.sqlite3_column_text(stmt, col);
    if (p == null) return "";
    const n: usize = @intCast(c.sqlite3_column_bytes(stmt, col));
    const bp: [*]const u8 = @ptrCast(p.?);
    return bp[0..n];
}

fn countTrades(db: *c.sqlite3) !i64 {
    var stmt: ?*c.sqlite3_stmt = null;
    const sql: [:0]const u8 = "SELECT COUNT(*) FROM trades;";

    var rc: c_int = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), rc);
    defer _ = c.sqlite3_finalize(stmt.?);

    rc = c.sqlite3_step(stmt.?);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_ROW), rc);

    return @as(i64, c.sqlite3_column_int64(stmt.?, 0));
}

test "insert_trade: valid insert round-trips values" {
    vprint("RUNNING: insert_trade: valid insert round-trips values");

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
    try execSql(db, schema_sql);

    const rc = api.zp_sqlite_insert_trade(
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
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(i64, 1), try countTrades(db));

    var stmt: ?*c.sqlite3_stmt = null;
    const q: [:0]const u8 =
        \\SELECT broker, trade_datetime, "type", ticker,
        \\       quantity, price_per_share, commission,
        \\       country, currency, conversion_rate_eur
        \\FROM trades WHERE ticker = ? LIMIT 1;
    ;

    var qrc: c_int = c.sqlite3_prepare_v2(db, q.ptr, -1, &stmt, null);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), qrc);
    defer _ = c.sqlite3_finalize(stmt.?);

    qrc = c.sqlite3_bind_text(stmt.?, 1, ticker_ge.ptr, -1, c.SQLITE_TRANSIENT);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), qrc);

    qrc = c.sqlite3_step(stmt.?);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_ROW), qrc);

    try std.testing.expectEqualStrings("IKBR", colText(stmt.?, 0));
    try std.testing.expectEqualStrings("2025-12-27 10:00", colText(stmt.?, 1));
    try std.testing.expectEqualStrings("SELL", colText(stmt.?, 2));
    try std.testing.expectEqualStrings("GE", colText(stmt.?, 3));

    try std.testing.expectApproxEqAbs(@as(f64, 10.0), c.sqlite3_column_double(stmt.?, 4), 1e-12);
    try std.testing.expectApproxEqAbs(@as(f64, 121.50), c.sqlite3_column_double(stmt.?, 5), 1e-12);
    try std.testing.expectApproxEqAbs(@as(f64, 1.23), c.sqlite3_column_double(stmt.?, 6), 1e-12);

    try std.testing.expectEqualStrings("US", colText(stmt.?, 7));
    try std.testing.expectEqualStrings("USD", colText(stmt.?, 8));
    try std.testing.expectApproxEqAbs(@as(f64, 1.0), c.sqlite3_column_double(stmt.?, 9), 1e-12);
}

test "insert_trade: invalid_argument when required fields are NULL" {
    vprint("RUNNING: insert_trade: invalid_argument when required fields are NULL");

    const mem: [:0]const u8 = ":memory:";
    const dt: [:0]const u8 = "2025-12-27 12:00";
    const ticker_ge: [:0]const u8 = "GE";
    const broker_ikbr: [:0]const u8 = "IKBR";
    const type_buy: [:0]const u8 = "BUY";
    const country_us: [:0]const u8 = "US";
    const currency_usd: [:0]const u8 = "USD";

    var handle: DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    // schema isn't needed for this check (it fails before DB usage)
    try std.testing.expectEqual(
        helper.ErrorCode.invalid_argument,
        api.zp_sqlite_insert_trade(
            handle,
            null, // trade_datetime required
            ticker_ge.ptr,
            1.0,
            100.0,
            broker_ikbr.ptr,
            type_buy.ptr,
            0.0,
            country_us.ptr,
            currency_usd.ptr,
            1.0,
        ),
    );

    try std.testing.expectEqual(
        helper.ErrorCode.invalid_argument,
        api.zp_sqlite_insert_trade(
            handle,
            dt.ptr,
            null, // ticker required
            1.0,
            100.0,
            broker_ikbr.ptr,
            type_buy.ptr,
            0.0,
            country_us.ptr,
            currency_usd.ptr,
            1.0,
        ),
    );
}

test "insert_trade: fails on invalid type CHECK constraint" {
    vprint("RUNNING: insert_trade: fails on invalid type CHECK constraint");

    const mem: [:0]const u8 = ":memory:";
    const dt: [:0]const u8 = "2025-12-27 12:00";
    const ticker_ge: [:0]const u8 = "GE";
    const broker_ikbr: [:0]const u8 = "IKBR";
    const type_hold: [:0]const u8 = "HOLD";
    const country_us: [:0]const u8 = "US";
    const currency_usd: [:0]const u8 = "USD";

    var handle: DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    const rc = api.zp_sqlite_insert_trade(
        handle,
        dt.ptr,
        ticker_ge.ptr,
        1.0,
        100.0,
        broker_ikbr.ptr,
        type_hold.ptr, // invalid by CHECK constraint
        0.0,
        country_us.ptr,
        currency_usd.ptr,
        1.0,
    );

    // Your sqlite_insert_trade maps sqlite3_step() constraint errors to .execution_fail
    try std.testing.expectEqual(helper.ErrorCode.execution_fail, rc);
}

test "insert_trade: defaults used when optional fields are NULL" {
    vprint("RUNNING: insert_trade: defaults used when optional fields are NULL");

    const mem: [:0]const u8 = ":memory:";
    const dt: [:0]const u8 = "2025-12-27 11:00";
    const ticker_ostk: [:0]const u8 = "OSTK";

    var handle: DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    // Assumes your INSERT uses COALESCE(...) so NULL triggers defaults, and
    // sqliteConnector normalizes NaN/<=0 numeric "defaults" to NULL binds.
    const rc = api.zp_sqlite_insert_trade(
        handle,
        dt.ptr,
        ticker_ostk.ptr,
        2.0,
        50.0,
        null, // broker -> default IKBR via COALESCE
        null, // type   -> default BUY via COALESCE
        std.math.nan(f64), // commission -> NULL -> default 0.0 via COALESCE
        null, // country -> default US via COALESCE
        null, // currency -> default USD via COALESCE
        0.0, // conversion_rate_eur -> NULL -> default 1.0 via COALESCE
    );
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);

    var stmt: ?*c.sqlite3_stmt = null;
    const q: [:0]const u8 =
        \\SELECT broker, "type", commission, country, currency, conversion_rate_eur
        \\FROM trades WHERE ticker = ? LIMIT 1;
    ;

    var qrc: c_int = c.sqlite3_prepare_v2(db, q.ptr, -1, &stmt, null);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), qrc);
    defer _ = c.sqlite3_finalize(stmt.?);

    qrc = c.sqlite3_bind_text(stmt.?, 1, ticker_ostk.ptr, -1, c.SQLITE_TRANSIENT);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), qrc);

    qrc = c.sqlite3_step(stmt.?);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_ROW), qrc);

    try std.testing.expectEqualStrings("IKBR", colText(stmt.?, 0));
    try std.testing.expectEqualStrings("BUY", colText(stmt.?, 1));
    try std.testing.expectApproxEqAbs(@as(f64, 0.0), c.sqlite3_column_double(stmt.?, 2), 1e-12);
    try std.testing.expectEqualStrings("US", colText(stmt.?, 3));
    try std.testing.expectEqualStrings("USD", colText(stmt.?, 4));
    try std.testing.expectApproxEqAbs(@as(f64, 1.0), c.sqlite3_column_double(stmt.?, 5), 1e-12);
}

test "insert_trade_struct: valid insert round-trips values" {
    vprint("RUNNING: insert_trade_struct: valid insert round-trips values");

    const mem: [:0]const u8 = ":memory:";
    const dt: [:0]const u8 = "2025-12-28 09:15";
    const ticker_ge: [:0]const u8 = "GE";
    const broker_ikbr: [:0]const u8 = "IKBR";
    const type_sell: [:0]const u8 = "SELL";
    const country_us: [:0]const u8 = "US";
    const currency_usd: [:0]const u8 = "USD";

    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    // ---- Build trade struct (choose the field name that matches your struct) ----
    var t: api.zp_trade = .{
        .trade_datetime = dt.ptr,
        .ticker = ticker_ge.ptr,
        .quantity = 10.0,
        .price_per_share = 121.50,
        .broker = broker_ikbr.ptr,
        // If your zp_trade uses @"type":
        .type = type_sell.ptr,
        // If your zp_trade uses trade_type instead, replace the line above with:
        // .trade_type = type_sell.ptr,
        .commission = 1.23,
        .country = country_us.ptr,
        .currency = currency_usd.ptr,
        .conversion_rate_eur = 1.0,
    };

    const rc = api.zp_sqlite_insert_trade_struct(handle, &t);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(i64, 1), try countTrades(db));

    // Verify inserted values via SQL
    var stmt: ?*c.sqlite3_stmt = null;
    const q: [:0]const u8 =
        \\SELECT broker, trade_datetime, "type", ticker,
        \\       quantity, price_per_share, commission,
        \\       country, currency, conversion_rate_eur
        \\FROM trades WHERE ticker = ? LIMIT 1;
    ;

    var qrc: c_int = c.sqlite3_prepare_v2(db, q.ptr, -1, &stmt, null);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), qrc);
    defer _ = c.sqlite3_finalize(stmt.?);

    qrc = c.sqlite3_bind_text(stmt.?, 1, ticker_ge.ptr, -1, c.SQLITE_TRANSIENT);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), qrc);

    qrc = c.sqlite3_step(stmt.?);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_ROW), qrc);

    try std.testing.expectEqualStrings("IKBR", colText(stmt.?, 0));
    try std.testing.expectEqualStrings("2025-12-28 09:15", colText(stmt.?, 1));
    try std.testing.expectEqualStrings("SELL", colText(stmt.?, 2));
    try std.testing.expectEqualStrings("GE", colText(stmt.?, 3));

    try std.testing.expectApproxEqAbs(@as(f64, 10.0), c.sqlite3_column_double(stmt.?, 4), 1e-12);
    try std.testing.expectApproxEqAbs(@as(f64, 121.50), c.sqlite3_column_double(stmt.?, 5), 1e-12);
    try std.testing.expectApproxEqAbs(@as(f64, 1.23), c.sqlite3_column_double(stmt.?, 6), 1e-12);

    try std.testing.expectEqualStrings("US", colText(stmt.?, 7));
    try std.testing.expectEqualStrings("USD", colText(stmt.?, 8));
    try std.testing.expectApproxEqAbs(@as(f64, 1.0), c.sqlite3_column_double(stmt.?, 9), 1e-12);
}

test "insert_trade_struct: invalid_argument when trade ptr is NULL" {
    vprint("RUNNING: insert_trade_struct: invalid_argument when trade ptr is NULL");

    const mem: [:0]const u8 = ":memory:";
    var handle: DbHandle = helper.INVALID_DB_HANDLE;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const rc = api.zp_sqlite_insert_trade_struct(handle, null);
    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, rc);
}

test "insert_trade_struct: invalid_argument when required fields are NULL" {
    vprint("RUNNING: insert_trade_struct: invalid_argument when required fields are NULL");

    const mem: [:0]const u8 = ":memory:";
    const dt: [:0]const u8 = "2025-12-28 09:15";
    const ticker_ge: [:0]const u8 = "GE";

    var handle: DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    // Missing trade_datetime
    var t1: api.zp_trade = .{
        .trade_datetime = null,
        .ticker = ticker_ge.ptr,
        .quantity = 1.0,
        .price_per_share = 100.0,
        .broker = null,
        .type = null, // or .trade_type = null
        .commission = 0.0,
        .country = null,
        .currency = null,
        .conversion_rate_eur = 1.0,
    };

    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, api.zp_sqlite_insert_trade_struct(handle, &t1));

    // Missing ticker
    var t2: api.zp_trade = .{
        .trade_datetime = dt.ptr,
        .ticker = null,
        .quantity = 1.0,
        .price_per_share = 100.0,
        .broker = null,
        .type = null, // or .trade_type = null
        .commission = 0.0,
        .country = null,
        .currency = null,
        .conversion_rate_eur = 1.0,
    };

    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, api.zp_sqlite_insert_trade_struct(handle, &t2));
}

test "insert_trade_struct: defaults used when optional fields are NULL / sentinels" {
    vprint("RUNNING: insert_trade_struct: defaults used when optional fields are NULL / sentinels");

    const mem: [:0]const u8 = ":memory:";
    const dt: [:0]const u8 = "2025-12-28 11:00";
    const ticker_ostk: [:0]const u8 = "OSTK";

    var handle: DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);

    var t: api.zp_trade = .{
        .trade_datetime = dt.ptr,
        .ticker = ticker_ostk.ptr,
        .quantity = 2.0,
        .price_per_share = 50.0,
        .broker = null, // default IKBR (via COALESCE)
        .type = null, // default BUY (via COALESCE)  (or .trade_type = null)
        .commission = std.math.nan(f64), // sentinel -> NULL bind -> default 0.0
        .country = null, // default US
        .currency = null, // default USD
        .conversion_rate_eur = 0.0, // sentinel -> NULL bind -> default 1.0
    };

    const rc = api.zp_sqlite_insert_trade_struct(handle, &t);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);

    var stmt: ?*c.sqlite3_stmt = null;
    const q: [:0]const u8 =
        \\SELECT broker, "type", commission, country, currency, conversion_rate_eur
        \\FROM trades WHERE ticker = ? LIMIT 1;
    ;

    var qrc: c_int = c.sqlite3_prepare_v2(db, q.ptr, -1, &stmt, null);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), qrc);
    defer _ = c.sqlite3_finalize(stmt.?);

    qrc = c.sqlite3_bind_text(stmt.?, 1, ticker_ostk.ptr, -1, c.SQLITE_TRANSIENT);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), qrc);

    qrc = c.sqlite3_step(stmt.?);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_ROW), qrc);

    try std.testing.expectEqualStrings("IKBR", colText(stmt.?, 0));
    try std.testing.expectEqualStrings("BUY", colText(stmt.?, 1));
    try std.testing.expectApproxEqAbs(@as(f64, 0.0), c.sqlite3_column_double(stmt.?, 2), 1e-12);
    try std.testing.expectEqualStrings("US", colText(stmt.?, 3));
    try std.testing.expectEqualStrings("USD", colText(stmt.?, 4));
    try std.testing.expectApproxEqAbs(@as(f64, 1.0), c.sqlite3_column_double(stmt.?, 5), 1e-12);
}
