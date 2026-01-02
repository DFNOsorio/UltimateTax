const std = @import("std");
const api = @import("api");
const helper = api.helper;

const common = @import("test_common.zig");

fn str(buf: []const u8) []const u8 {
    return std.mem.sliceTo(buf, 0);
}

test "read_all_trades: returns all rows ordered by datetime" {
    common.vprint("read_all_trades: returns all rows ordered by datetime");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    _ = api.zp_sqlite_insert_trade(handle, "2025-01-01 10:00".ptr, "TSLA".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2024-01-01 10:00".ptr, "GE".ptr, 1, 1, "IKBR".ptr, "BUY".ptr, 0, "US".ptr, "USD".ptr, 1);
    _ = api.zp_sqlite_insert_trade(handle, "2024-06-01 10:00".ptr, "AAPL".ptr, 1, 1, "XTB".ptr, "SELL".ptr, 0, "US".ptr, "USD".ptr, 1);

    var needed: usize = 0;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_read_all_trades(handle, null, 0, &needed));
    try std.testing.expectEqual(@as(usize, 3), needed);

    var buf: [8]api.schema.zp_trade = undefined;
    var written: usize = 0;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_read_all_trades(handle, buf[0..].ptr, buf.len, &written));
    try std.testing.expectEqual(@as(usize, 3), written);

    // Ordered by datetime asc
    try std.testing.expectEqualStrings("2024-01-01 10:00", str(buf[0].trade_datetime[0..]));
    try std.testing.expectEqualStrings("2024-06-01 10:00", str(buf[1].trade_datetime[0..]));
    try std.testing.expectEqualStrings("2025-01-01 10:00", str(buf[2].trade_datetime[0..]));
}
