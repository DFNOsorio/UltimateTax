const std = @import("std");
const api = @import("api");
const helper = api.helper;

const common = @import("test_common.zig");

fn str(buf: []const u8) []const u8 {
    return std.mem.sliceTo(buf, 0);
}

test "read_trade_by_id: returns trade struct for existing row" {
    common.vprint("read_trade_by_id: returns trade struct for existing row");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    // Insert one row => id == 1
    try std.testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_insert_trade(
            handle,
            "2025-01-02 10:00".ptr,
            "GE".ptr,
            10.0,
            100.0,
            "IKBR".ptr,
            "SELL".ptr,
            1.0,
            "US".ptr,
            "USD".ptr,
            1.0,
        ),
    );

    var out: api.schema.zp_trade = undefined;
    api.schema.clearTrade(&out);

    const rc = api.zp_sqlite_read_trade_by_id(handle, 1, &out);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);

    try std.testing.expectEqualStrings("IKBR", str(out.broker[0..]));
    try std.testing.expectEqualStrings("2025-01-02 10:00", str(out.trade_datetime[0..]));
    try std.testing.expectEqualStrings("SELL", str(out.trade_type[0..]));
    try std.testing.expectEqualStrings("GE", str(out.ticker[0..]));
    try std.testing.expectApproxEqAbs(@as(f64, 10.0), out.quantity, 1e-12);
}

test "read_trade_by_id: returns execution_fail when not found" {
    common.vprint("read_trade_by_id: returns execution_fail when not found");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    var out: api.schema.zp_trade = undefined;
    api.schema.clearTrade(&out);

    const rc = api.zp_sqlite_read_trade_by_id(handle, 999, &out);
    try std.testing.expectEqual(helper.ErrorCode.execution_fail, rc);
}
