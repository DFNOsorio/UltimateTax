const std = @import("std");
const api = @import("api");
const helper = api.helper;

const common = @import("test_common.zig");

fn zstr(buf: []const u8) []const u8 {
    return std.mem.sliceTo(buf, 0);
}

fn seed_dividends(handle: helper.DbHandle) void {
    _ = api.zp_sqlite_insert_dividend(handle, "IKBR".ptr, "2024-01-01 00:00".ptr, "AAPL".ptr, "US".ptr, 0.5, 10.0, 1.0, "USD".ptr, 1.0);

    _ = api.zp_sqlite_insert_dividend(handle, "IKBR".ptr, "2024-06-01 00:00".ptr, "MSFT".ptr, "US".ptr, 1.0, 20.0, 2.0, "USD".ptr, 1.0);

    _ = api.zp_sqlite_insert_dividend(handle, "DEGIRO".ptr, "2024-03-01 00:00".ptr, "EDP".ptr, "PT".ptr, 0.2, 5.0, 0.5, "EUR".ptr, 1.0);

    _ = api.zp_sqlite_insert_dividend(handle, "XTB".ptr, "2025-01-01 00:00".ptr, "TSLA".ptr, "US".ptr, 0.1, 7.0, 0.0, "USD".ptr, 1.0);
}

test "dividends count/read by year" {
    common.vprint("dividends count/read by year");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    seed_dividends(handle);

    var n: usize = 0;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_count_dividends_by_year(handle, 2024, &n));
    try std.testing.expectEqual(@as(usize, 3), n);

    var out_n: usize = 0;
    var buf: [8]api.schema.zp_dividend = undefined;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_read_dividends_by_year(handle, 2024, buf[0..].ptr, buf.len, &out_n));
    try std.testing.expectEqual(@as(usize, 3), out_n);

    // ordered by dt
    try std.testing.expectEqualStrings("2024-01-01 00:00", zstr(buf[0].dividend_dt[0..]));
    try std.testing.expectEqualStrings("2024-03-01 00:00", zstr(buf[1].dividend_dt[0..]));
    try std.testing.expectEqualStrings("2024-06-01 00:00", zstr(buf[2].dividend_dt[0..]));
}

test "dividends count/read by country" {
    common.vprint("dividends count/read by country");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    seed_dividends(handle);

    var n: usize = 0;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_count_dividends_by_country(handle, "US".ptr, &n));
    try std.testing.expectEqual(@as(usize, 3), n);

    var out_n: usize = 0;
    var buf: [8]api.schema.zp_dividend = undefined;

    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_read_dividends_by_country(handle, "PT".ptr, buf[0..].ptr, buf.len, &out_n));
    try std.testing.expectEqual(@as(usize, 1), out_n);

    try std.testing.expectEqualStrings("PT", zstr(buf[0].country[0..]));
    try std.testing.expectEqualStrings("EDP", zstr(buf[0].ticker[0..]));
}

test "dividends count/read by year and country" {
    common.vprint("dividends count/read by year and country");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    seed_dividends(handle);

    var n: usize = 0;
    try std.testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_count_dividends_by_year_and_country(handle, 2024, "US".ptr, &n),
    );
    try std.testing.expectEqual(@as(usize, 2), n);

    var out_n: usize = 0;
    var buf: [8]api.schema.zp_dividend = undefined;

    try std.testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_read_dividends_by_year_and_country(handle, 2024, "US".ptr, buf[0..].ptr, buf.len, &out_n),
    );
    try std.testing.expectEqual(@as(usize, 2), out_n);

    try std.testing.expectEqualStrings("US", zstr(buf[0].country[0..]));
    try std.testing.expectEqualStrings("US", zstr(buf[1].country[0..]));
    try std.testing.expectEqualStrings("2024-01-01 00:00", zstr(buf[0].dividend_dt[0..]));
    try std.testing.expectEqualStrings("2024-06-01 00:00", zstr(buf[1].dividend_dt[0..]));
}

test "dividends filters: invalid_argument when required pointers are NULL" {
    common.vprint("dividends filters: invalid_argument when required pointers are NULL");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    var n: usize = 0;

    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, api.zp_sqlite_count_dividends_by_country(handle, null, &n));
    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, api.zp_sqlite_count_dividends_by_year(handle, 2024, null));
    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, api.zp_sqlite_count_dividends_by_year_and_country(handle, 2024, null, &n));

    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, api.zp_sqlite_read_dividends_by_country(handle, null, null, 0, &n));
    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, api.zp_sqlite_read_dividends_by_year(handle, 2024, null, 0, null));
    try std.testing.expectEqual(helper.ErrorCode.invalid_argument, api.zp_sqlite_read_dividends_by_year_and_country(handle, 2024, null, null, 0, &n));
}
