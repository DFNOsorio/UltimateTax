const std = @import("std");
const api = @import("api");
const tc = @import("test_common.zig");

test "fifo_realized: insert + read filters" {
    tc.vprint("fifo_realized: insert + read filters");

    const handle = try tc.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    try tc.ensureFifoTables(handle);

    const buy_id_2024 = try tc.insertTradeReturnId(handle, "2024-01-01 10:00", "AAPL", "BUY");
    const sell_id_2024 = try tc.insertTradeReturnId(handle, "2024-06-01 10:00", "AAPL", "SELL");

    const buy_id_2025 = try tc.insertTradeReturnId(handle, "2025-01-10 10:00", "MSFT", "BUY");
    const sell_id_2025 = try tc.insertTradeReturnId(handle, "2025-07-10 10:00", "MSFT", "SELL");

    var r1 = api.schema.zp_fifo_realized.zero();
    tc.setBufZ(r1.broker[0..], "IKBR");
    r1.tax_year = 2024;
    tc.setBufZ(r1.ticker[0..], "AAPL");
    r1.sell_trade_id = sell_id_2024;
    r1.buy_trade_id = buy_id_2024;
    r1.match_seq = 1;
    tc.setBufZ(r1.sell_datetime[0..], "2024-06-01 10:00");
    tc.setBufZ(r1.buy_datetime[0..], "2024-01-01 10:00");
    r1.qty_matched = 1.0;
    r1.proceeds_eur = 120.0;
    r1.cost_eur = 100.0;
    r1.gain_eur = 20.0;

    var r2 = api.schema.zp_fifo_realized.zero();
    tc.setBufZ(r2.broker[0..], "DEGIRO");
    r2.tax_year = 2025;
    tc.setBufZ(r2.ticker[0..], "MSFT");
    r2.sell_trade_id = sell_id_2025;
    r2.buy_trade_id = buy_id_2025;
    r2.match_seq = 1;
    tc.setBufZ(r2.sell_datetime[0..], "2025-07-10 10:00");
    tc.setBufZ(r2.buy_datetime[0..], "2025-01-10 10:00");
    r2.qty_matched = 1.0;
    r2.proceeds_eur = 200.0;
    r2.cost_eur = 150.0;
    r2.gain_eur = 50.0;

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_insert_fifo_realized(handle, &r1));
    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_insert_fifo_realized(handle, &r2));

    // read_all
    var out: [8]api.schema.zp_fifo_realized = undefined;
    var count: usize = 0;

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_realized_all(handle, &out, out.len, &count));
    try std.testing.expectEqual(@as(usize, 2), count);

    // read_by_tax_year
    var out_y: [8]api.schema.zp_fifo_realized = undefined;
    var count_y: usize = 0;

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_realized_by_tax_year(handle, 2024, &out_y, out_y.len, &count_y));
    try std.testing.expectEqual(@as(usize, 1), count_y);
    try std.testing.expectEqualStrings("AAPL", tc.zstr(out_y[0].ticker[0..]));

    // read_by_ticker_per_year
    var out_t: [8]api.schema.zp_fifo_realized = undefined;
    var count_t: usize = 0;

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_realized_by_ticker_per_year(handle, 2025, "MSFT", &out_t, out_t.len, &count_t));
    try std.testing.expectEqual(@as(usize, 1), count_t);
    try std.testing.expectEqualStrings("DEGIRO", tc.zstr(out_t[0].broker[0..]));

    // read_by_broker_per_year
    var out_b: [8]api.schema.zp_fifo_realized = undefined;
    var count_b: usize = 0;

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_realized_by_broker_per_year(handle, 2024, "IKBR", &out_b, out_b.len, &count_b));
    try std.testing.expectEqual(@as(usize, 1), count_b);
    try std.testing.expectEqualStrings("AAPL", tc.zstr(out_b[0].ticker[0..]));
}
