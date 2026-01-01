const std = @import("std");
const api = @import("api");
const helper = api.helper;
const sqlite = api.sqlite;
const c = sqlite.c;

const common = @import("test_common.zig");

test "insert_trade: valid insert round-trips values" {
    common.vprint("RUNNING: insert_trade: valid insert round-trips values");

    const dt: [:0]const u8 = "2025-12-27 10:00";
    const ticker_ge: [:0]const u8 = "GE";
    const broker_ikbr: [:0]const u8 = "IKBR";
    const type_sell: [:0]const u8 = "SELL";
    const country_us: [:0]const u8 = "US";
    const currency_usd: [:0]const u8 = "USD";

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);

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
    try std.testing.expectEqual(@as(i64, 1), try common.countTrades(db));
}

test "insert_trade_struct: inserts using zp_trade buffers" {
    common.vprint("RUNNING: insert_trade_struct: inserts using zp_trade buffers");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    var t: api.trade.zp_trade = undefined;
    api.trade.clearTrade(&t);

    common.setBufZ(t.trade_datetime[0..], "2025-12-27 09:00");
    common.setBufZ(t.ticker[0..], "AAPL");

    t.quantity = 2.0;
    t.price_per_share = 200.0;

    // Defaultable fields: keep empty -> defaults, but set one explicitly to ensure it round-trips.
    common.setBufZ(t.broker[0..], "XTB");
    common.setBufZ(t.trade_type[0..], "BUY");

    t.commission = 0.5;
    common.setBufZ(t.country[0..], "US");
    common.setBufZ(t.currency[0..], "USD");
    t.conversion_rate_eur = 1.0;

    const rc = api.zp_sqlite_insert_trade_struct(handle, &t);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
}

test "insert_trade: invalid_argument when required fields are NULL" {
    common.vprint("RUNNING: insert_trade: invalid_argument when required fields are NULL");

    const mem: [:0]const u8 = ":memory:";
    var handle: helper.DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    try std.testing.expectEqual(
        helper.ErrorCode.invalid_argument,
        api.zp_sqlite_insert_trade(
            handle,
            null,
            "GE".ptr,
            1.0,
            100.0,
            "IKBR".ptr,
            "BUY".ptr,
            0.0,
            "US".ptr,
            "USD".ptr,
            1.0,
        ),
    );

    try std.testing.expectEqual(
        helper.ErrorCode.invalid_argument,
        api.zp_sqlite_insert_trade(
            handle,
            "2025-12-27 12:00".ptr,
            null,
            1.0,
            100.0,
            "IKBR".ptr,
            "BUY".ptr,
            0.0,
            "US".ptr,
            "USD".ptr,
            1.0,
        ),
    );
}

test "insert_trade: fails on invalid type CHECK constraint" {
    common.vprint("RUNNING: insert_trade: fails on invalid type CHECK constraint");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    const rc = api.zp_sqlite_insert_trade(
        handle,
        "2025-12-27 12:00".ptr,
        "GE".ptr,
        1.0,
        100.0,
        "IKBR".ptr,
        "HOLD".ptr, // invalid
        0.0,
        "US".ptr,
        "USD".ptr,
        1.0,
    );

    try std.testing.expectEqual(helper.ErrorCode.execution_fail, rc);
}

test "insert_trade: defaults used when optional fields are NULL" {
    common.vprint("RUNNING: insert_trade: defaults used when optional fields are NULL");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    const rc = api.zp_sqlite_insert_trade(
        handle,
        "2025-12-27 11:00".ptr,
        "OSTK".ptr,
        2.0,
        50.0,
        null,
        null,
        std.math.nan(f64),
        null,
        null,
        0.0,
    );
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
}
