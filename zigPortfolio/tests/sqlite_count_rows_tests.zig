const std = @import("std");
const testing = std.testing;

const api = @import("api");
const helper = api.helper;

const common = @import("test_common.zig");

test "sqlite_count_rows – trades" {
    common.vprint("sqlite_count_rows: trades");
    const db = try common.openMemDb();
    defer _ = api.zp_sqlite_close(db);

    // Insert trades
    _ = try common.insertTradeReturnId(db, "2022-01-10", "AAPL", "BUY");
    _ = try common.insertTradeReturnId(db, "2022-06-01", "MSFT", "BUY");
    _ = try common.insertTradeReturnId(db, "2023-02-15", "AAPL", "BUY");

    var n: usize = 0;
    var y2022: u32 = 2022;

    // all trades
    try testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_rows(
            db,
            api.zp_table.trades,
            null,
            null,
            null,
            &n,
        ),
    );
    try testing.expectEqual(@as(usize, 3), n);

    // trades by year
    try testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_rows(
            db,
            api.zp_table.trades,
            &y2022,
            null,
            null,
            &n,
        ),
    );
    try testing.expectEqual(@as(usize, 2), n);

    // trades by ticker
    try testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_rows(
            db,
            api.zp_table.trades,
            null,
            null,
            "AAPL",
            &n,
        ),
    );
    try testing.expectEqual(@as(usize, 2), n);
}

test "sqlite_count_rows – fifo tables" {
    common.vprint("sqlite_count_rows: fifo tables");

    const db = try common.openMemDb();
    defer _ = api.zp_sqlite_close(db);

    try common.ensureFifoTables(db);

    // Trades needed for FK references
    _ = try common.insertTradeReturnId(db, "2022-01-01", "AAPL", "BUY");
    _ = try common.insertTradeReturnId(db, "2022-06-01", "AAPL", "SELL");

    const c = api.sqlite.c;
    const db_ptr: *c.sqlite3 = @ptrFromInt(db);

    // fifo_snapshot row
    try common.execSql(db_ptr,
        \\INSERT INTO fifo_snapshot
        \\(broker, tax_year, ticker, acq_trade_id, acq_datetime,
        \\ qty_remaining, cost_per_share_eur, country)
        \\VALUES ('IBKR', 2022, 'AAPL', 1, '2022-01-01', 10.0, 100.0, 'US');
    );

    // fifo_realized row
    try common.execSql(db_ptr,
        \\INSERT INTO fifo_realized
        \\(broker, tax_year, ticker, sell_trade_id, buy_trade_id,
        \\ match_seq, sell_datetime, buy_datetime,
        \\ qty_matched, proceeds_eur, cost_eur, gain_eur)
        \\VALUES ('IBKR', 2022, 'AAPL', 2, 1, 1,
        \\ '2022-06-01', '2022-01-01', 10.0, 1200.0, 1000.0, 200.0);
    );

    var n: usize = 0;
    var y2022: u32 = 2022;

    // fifo_snapshot by year
    try testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_rows(
            db,
            api.zp_table.fifo_snapshot,
            &y2022,
            null,
            null,
            &n,
        ),
    );
    try testing.expectEqual(@as(usize, 1), n);

    // fifo_realized by broker
    try testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_rows(
            db,
            api.zp_table.fifo_realized,
            null,
            "IBKR",
            null,
            &n,
        ),
    );
    try testing.expectEqual(@as(usize, 1), n);
}

test "sqlite_count_trades_by_year_side – buy/sell" {
    common.vprint("sqlite_count_trades_by_year_side: buy/sell");

    const db = try common.openMemDb();
    defer _ = api.zp_sqlite_close(db);

    // Mix of BUY/SELL across years
    _ = try common.insertTradeReturnId(db, "2022-01-10", "AAPL", "BUY");
    _ = try common.insertTradeReturnId(db, "2022-02-11", "AAPL", "SELL");
    _ = try common.insertTradeReturnId(db, "2022-06-01", "MSFT", "BUY");
    _ = try common.insertTradeReturnId(db, "2023-02-15", "AAPL", "BUY");
    _ = try common.insertTradeReturnId(db, "2023-03-20", "MSFT", "SELL");

    var n: usize = 0;

    // 2022 BUY = 2
    try testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_buy_trades_by_year(db, 2022, &n),
    );
    try testing.expectEqual(@as(usize, 2), n);

    // 2022 SELL = 1
    try testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_sell_trades_by_year(db, 2022, &n),
    );
    try testing.expectEqual(@as(usize, 1), n);

    // 2023 BUY = 1
    try testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_buy_trades_by_year(db, 2023, &n),
    );
    try testing.expectEqual(@as(usize, 1), n);

    // 2023 SELL = 1
    try testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_sell_trades_by_year(db, 2023, &n),
    );
    try testing.expectEqual(@as(usize, 1), n);

    // 2024 BUY = 0 (no rows)
    try testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_buy_trades_by_year(db, 2024, &n),
    );
    try testing.expectEqual(@as(usize, 0), n);

    // 2024 SELL = 0 (no rows)
    try testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_sell_trades_by_year(db, 2024, &n),
    );
    try testing.expectEqual(@as(usize, 0), n);
}

test "sqlite_count_trades_by_year_side – invalid args" {
    common.vprint("sqlite_count_trades_by_year_side: invalid args");

    const db = try common.openMemDb();
    defer _ = api.zp_sqlite_close(db);

    var n: usize = 123;

    // year == 0 should be invalid_argument (per sqliteMeta.zig helper)
    try testing.expectEqual(
        helper.ErrorCode.invalid_argument,
        api.zp_sqlite_count_buy_trades_by_year(db, 0, &n),
    );

    // null out_count should map to preparation_fail in the connector wrapper you added
    try testing.expectEqual(
        helper.ErrorCode.preparation_fail,
        api.zp_sqlite_count_sell_trades_by_year(db, 2022, null),
    );
}
