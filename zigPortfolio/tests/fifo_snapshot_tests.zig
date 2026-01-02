const std = @import("std");
const api = @import("api");
const tc = @import("test_common.zig");

test "fifo_snapshot: insert + read filters" {
    tc.vprint("fifo_snapshot: insert + read filters");

    const handle = try tc.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    try tc.ensureFifoTables(handle);

    const buy_id_2024 = try tc.insertTradeReturnId(handle, "2024-01-01 10:00", "AAPL", "BUY");
    const buy_id_2025 = try tc.insertTradeReturnId(handle, "2025-02-01 10:00", "MSFT", "BUY");

    var r1 = api.schema.zp_fifo_snapshot.zero();
    tc.setBufZ(r1.broker[0..], "IKBR");
    r1.tax_year = 2024;
    tc.setBufZ(r1.ticker[0..], "AAPL");
    r1.acq_trade_id = buy_id_2024;
    tc.setBufZ(r1.acq_datetime[0..], "2024-01-01 10:00");
    r1.qty_remaining = 1.0;
    r1.cost_per_share_eur = 90.0;
    r1.acq_commission_eur = std.math.nan(f64); // allow default if your binder uses NaN->NULL->COALESCE
    tc.setBufZ(r1.country[0..], "US");

    var r2 = api.schema.zp_fifo_snapshot.zero();
    tc.setBufZ(r2.broker[0..], "DEGIRO");
    r2.tax_year = 2025;
    tc.setBufZ(r2.ticker[0..], "MSFT");
    r2.acq_trade_id = buy_id_2025;
    tc.setBufZ(r2.acq_datetime[0..], "2025-02-01 10:00");
    r2.qty_remaining = 2.0;
    r2.cost_per_share_eur = 150.0;
    r2.acq_commission_eur = 0.0;
    tc.setBufZ(r2.country[0..], "US");

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_insert_fifo_snapshot(handle, &r1));
    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_insert_fifo_snapshot(handle, &r2));

    // read_all
    var out: [8]api.schema.zp_fifo_snapshot = undefined;
    var count: usize = 0;

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_snapshot_all(handle, &out, out.len, &count));
    try std.testing.expectEqual(@as(usize, 2), count);

    // read_by_tax_year
    var out_y: [8]api.schema.zp_fifo_snapshot = undefined;
    var count_y: usize = 0;

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_snapshot_by_tax_year(handle, 2024, &out_y, out_y.len, &count_y));
    try std.testing.expectEqual(@as(usize, 1), count_y);
    try std.testing.expectEqualStrings("AAPL", tc.zstr(out_y[0].ticker[0..]));

    // read_by_ticker_per_year
    var out_t: [8]api.schema.zp_fifo_snapshot = undefined;
    var count_t: usize = 0;

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_snapshot_by_ticker_per_year(handle, 2025, "MSFT", &out_t, out_t.len, &count_t));
    try std.testing.expectEqual(@as(usize, 1), count_t);
    try std.testing.expectEqualStrings("DEGIRO", tc.zstr(out_t[0].broker[0..]));

    // read_by_broker_per_year
    var out_b: [8]api.schema.zp_fifo_snapshot = undefined;
    var count_b: usize = 0;

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_snapshot_by_broker_per_year(handle, 2024, "IKBR", &out_b, out_b.len, &count_b));
    try std.testing.expectEqual(@as(usize, 1), count_b);
    try std.testing.expectEqualStrings("AAPL", tc.zstr(out_b[0].ticker[0..]));
}
