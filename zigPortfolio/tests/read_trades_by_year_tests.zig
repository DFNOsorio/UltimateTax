const std = @import("std");
const api = @import("api");
const helper = api.helper;

const common = @import("test_common.zig");

fn str(buf: []const u8) []const u8 {
    return std.mem.sliceTo(buf, 0);
}

test "read_trades_by_year: returns only rows for the given year" {
    common.vprint("read_trades_by_year: returns only rows for the given year");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    // Insert 3 rows: 2 in 2024, 1 in 2025
    _ = api.zp_sqlite_insert_trade(handle, "2024-01-01 10:00".ptr, "GE".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2024-06-01 10:00".ptr, "AAPL".ptr, 1, 1, "XTB".ptr, "SELL".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2025-01-01 10:00".ptr, "MSFT".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);

    var needed: usize = 0;
    const rc_count = api.zp_sqlite_read_trades_by_year(handle, 2024, null, 0, &needed);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc_count);
    try std.testing.expectEqual(@as(usize, 2), needed);

    var buf: [8]api.schema.zp_trade = undefined;
    var written: usize = 0;
    const rc = api.zp_sqlite_read_trades_by_year(handle, 2024, buf[0..].ptr, buf.len, &written);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 2), written);

    try std.testing.expectEqualStrings("2024-01-01 10:00", str(buf[0].trade_datetime[0..]));
    try std.testing.expectEqualStrings("2024-06-01 10:00", str(buf[1].trade_datetime[0..]));
}

test "read_buy_trades_by_year: returns only BUY rows for the given year" {
    common.vprint("read_buy_trades_by_year: returns only BUY rows for the given year");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    _ = api.zp_sqlite_insert_trade(handle, "2024-01-01 10:00".ptr, "GE".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2024-06-01 10:00".ptr, "AAPL".ptr, 1, 1, "XTB".ptr, "SELL".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2024-07-01 10:00".ptr, "MSFT".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2025-01-01 10:00".ptr, "TSLA".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);

    var needed: usize = 0;
    const rc_count = api.zp_sqlite_read_buy_trades_by_year(handle, 2024, null, 0, &needed);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc_count);
    try std.testing.expectEqual(@as(usize, 2), needed);

    var buf: [8]api.schema.zp_trade = undefined;
    var written: usize = 0;
    const rc = api.zp_sqlite_read_buy_trades_by_year(handle, 2024, buf[0..].ptr, buf.len, &written);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 2), written);

    try std.testing.expectEqualStrings("BUY", str(buf[0].trade_type[0..]));
    try std.testing.expectEqualStrings("BUY", str(buf[1].trade_type[0..]));
    try std.testing.expectEqualStrings("2024-01-01 10:00", str(buf[0].trade_datetime[0..]));
    try std.testing.expectEqualStrings("2024-07-01 10:00", str(buf[1].trade_datetime[0..]));
}

test "read_sell_trades_by_year: returns only SELL rows for the given year" {
    common.vprint("read_sell_trades_by_year: returns only SELL rows for the given year");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    _ = api.zp_sqlite_insert_trade(handle, "2024-01-01 10:00".ptr, "GE".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2024-06-01 10:00".ptr, "AAPL".ptr, 1, 1, "XTB".ptr, "SELL".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2024-07-01 10:00".ptr, "MSFT".ptr, 1, 1, "IKBR".ptr, "SELL".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2025-01-01 10:00".ptr, "TSLA".ptr, 1, 1, "IKBR".ptr, "SELL".ptr, 0, "US".ptr, "USD".ptr, 1);

    var needed: usize = 0;
    const rc_count = api.zp_sqlite_read_sell_trades_by_year(handle, 2024, null, 0, &needed);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc_count);
    try std.testing.expectEqual(@as(usize, 2), needed);

    var buf: [8]api.schema.zp_trade = undefined;
    var written: usize = 0;
    const rc = api.zp_sqlite_read_sell_trades_by_year(handle, 2024, buf[0..].ptr, buf.len, &written);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 2), written);

    try std.testing.expectEqualStrings("SELL", str(buf[0].trade_type[0..]));
    try std.testing.expectEqualStrings("SELL", str(buf[1].trade_type[0..]));
    try std.testing.expectEqualStrings("2024-06-01 10:00", str(buf[0].trade_datetime[0..]));
    try std.testing.expectEqualStrings("2024-07-01 10:00", str(buf[1].trade_datetime[0..]));
}
