const std = @import("std");
const api = @import("api");
const helper = api.helper;

const common = @import("test_common.zig");

fn str(buf: []const u8) []const u8 {
    return std.mem.sliceTo(buf, 0);
}

test "get_unique_brokers: returns distinct broker list" {
    common.vprint("RUNNING: get_unique_brokers: returns distinct broker list");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    _ = api.zp_sqlite_insert_trade(handle, "2024-01-01 10:00".ptr, "GE".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2024-02-01 10:00".ptr, "AAPL".ptr, 1, 1, "XTB".ptr, "SELL".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2025-01-01 10:00".ptr, "MSFT".ptr, 1, 1, "DEGIRO".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);

    var needed: usize = 0;
    const rc_count = api.zp_sqlite_get_unique_brokers(handle, null, 0, &needed);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc_count);
    try std.testing.expectEqual(@as(usize, 3), needed);

    var brokers: [8]api.trade.zp_broker_name = undefined;
    var written: usize = 0;
    const rc = api.zp_sqlite_get_unique_brokers(handle, brokers[0..].ptr, brokers.len, &written);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 3), written);

    // Sorted ASC by broker
    try std.testing.expectEqualStrings("DEGIRO", str(brokers[0].name[0..]));
    try std.testing.expectEqualStrings("IKBR", str(brokers[1].name[0..]));
    try std.testing.expectEqualStrings("XTB", str(brokers[2].name[0..]));
}

test "get_unique_years: returns distinct year list" {
    common.vprint("RUNNING: get_unique_years: returns distinct year list");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    _ = api.zp_sqlite_insert_trade(handle, "2024-01-01 10:00".ptr, "GE".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2025-02-01 10:00".ptr, "AAPL".ptr, 1, 1, "XTB".ptr, "SELL".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2023-01-01 10:00".ptr, "MSFT".ptr, 1, 1, "DEGIRO".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);

    var needed: usize = 0;
    const rc_count = api.zp_sqlite_get_unique_years(handle, null, 0, &needed);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc_count);
    try std.testing.expectEqual(@as(usize, 3), needed);

    var years: [8]u32 = undefined;
    var written: usize = 0;
    const rc = api.zp_sqlite_get_unique_years(handle, years[0..].ptr, years.len, &written);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 3), written);

    try std.testing.expectEqual(@as(u32, 2023), years[0]);
    try std.testing.expectEqual(@as(u32, 2024), years[1]);
    try std.testing.expectEqual(@as(u32, 2025), years[2]);
}

test "get_unique_brokers: truncates safely when out_cap is too small" {
    common.vprint("RUNNING: get_unique_brokers: truncates safely when out_cap is too small");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    _ = api.zp_sqlite_insert_trade(handle, "2024-01-01 10:00".ptr, "GE".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2024-02-01 10:00".ptr, "AAPL".ptr, 1, 1, "XTB".ptr, "SELL".ptr, 0, "US".ptr, "USD".ptr, 1);

    var brokers: [1]api.trade.zp_broker_name = undefined;
    var written: usize = 0;
    const rc = api.zp_sqlite_get_unique_brokers(handle, brokers[0..].ptr, brokers.len, &written);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 1), written);
}

test "get_unique_years: truncates safely when out_cap is too small" {
    common.vprint("RUNNING: get_unique_years: truncates safely when out_cap is too small");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    _ = api.zp_sqlite_insert_trade(handle, "2024-01-01 10:00".ptr, "GE".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2025-02-01 10:00".ptr, "AAPL".ptr, 1, 1, "XTB".ptr, "SELL".ptr, 0, "US".ptr, "USD".ptr, 1);

    var years: [1]u32 = undefined;
    var written: usize = 0;
    const rc = api.zp_sqlite_get_unique_years(handle, years[0..].ptr, years.len, &written);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);
    try std.testing.expectEqual(@as(usize, 1), written);
}
