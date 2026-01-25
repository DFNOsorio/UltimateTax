const std = @import("std");
const api = @import("api");
const helper = api.helper;

const common = @import("test_common.zig");

fn str(buf: []const u8) []const u8 {
    return std.mem.sliceTo(buf, 0);
}

test "count_dividends: returns correct COUNT(*)" {
    common.vprint("count_dividends: returns correct COUNT(*)");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    var n: usize = 1234;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_count_dividends(handle, &n));
    try std.testing.expectEqual(@as(usize, 0), n);

    _ = api.zp_sqlite_insert_dividend(
        handle,
        "IKBR".ptr,
        "2024-01-01 00:00".ptr,
        "AAPL".ptr,
        "US".ptr,
        0.5,
        10.0,
        1.0,
        "USD".ptr,
        1.0,
    );

    _ = api.zp_sqlite_insert_dividend(
        handle,
        "DEGIRO".ptr,
        "2025-01-01 00:00".ptr,
        "MSFT".ptr,
        "US".ptr,
        1.0,
        20.0,
        2.0,
        "USD".ptr,
        1.0,
    );

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_count_dividends(handle, &n));
    try std.testing.expectEqual(@as(usize, 2), n);
}

test "read_all_dividends: returns all rows ordered by dividend_dt" {
    common.vprint("read_all_dividends: returns all rows ordered by dividend_dt");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    _ = api.zp_sqlite_insert_dividend(
        handle,
        "IKBR".ptr,
        "2025-01-01 00:00".ptr,
        "TSLA".ptr,
        "US".ptr,
        1.0,
        10.0,
        0.5,
        "USD".ptr,
        1.0,
    );
    _ = api.zp_sqlite_insert_dividend(
        handle,
        "IKBR".ptr,
        "2024-01-01 00:00".ptr,
        "AAPL".ptr,
        "US".ptr,
        0.5,
        20.0,
        1.0,
        "USD".ptr,
        1.0,
    );
    _ = api.zp_sqlite_insert_dividend(
        handle,
        "XTB".ptr,
        "2024-06-01 00:00".ptr,
        "GE".ptr,
        "US".ptr,
        2.0,
        30.0,
        3.0,
        "USD".ptr,
        1.0,
    );

    // separate count function
    var total: usize = 0;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_count_dividends(handle, &total));
    try std.testing.expectEqual(@as(usize, 3), total);

    // read list
    var written: usize = 0;
    var buf: [8]api.schema.zp_dividend = undefined;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_read_all_dividends(handle, buf[0..].ptr, buf.len, &written));
    try std.testing.expectEqual(@as(usize, 3), written);

    // Ordered by dividend_dt asc
    try std.testing.expectEqualStrings("2024-01-01 00:00", str(buf[0].dividend_dt[0..]));
    try std.testing.expectEqualStrings("2024-06-01 00:00", str(buf[1].dividend_dt[0..]));
    try std.testing.expectEqualStrings("2025-01-01 00:00", str(buf[2].dividend_dt[0..]));

    // Spot-check fields
    try std.testing.expectEqualStrings("AAPL", str(buf[0].ticker[0..]));
    try std.testing.expectApproxEqAbs(@as(f64, 20.0), buf[0].total_amount, 1e-12);
}

test "dividends: invalid_argument when required pointers are NULL" {
    common.vprint("dividends: invalid_argument when required pointers are NULL");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, api.zp_sqlite_count_dividends(handle, null));

    var n: usize = 0;
    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, api.zp_sqlite_read_all_dividends(handle, null, 0, null));
    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, api.zp_sqlite_read_all_dividends(helper.INVALID_DB_HANDLE, null, 0, &n));
}
