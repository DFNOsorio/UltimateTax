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

test "fifo_snapshot: read by year+broker+ticker" {
    tc.vprint("fifo_snapshot: read by year+broker+ticker");

    const handle = try tc.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    try tc.ensureFifoTables(handle);

    const buy_id_2024 = try tc.insertTradeReturnId(handle, "2024-01-01 10:00", "AAPL", "BUY");
    const buy_id_2025 = try tc.insertTradeReturnId(handle, "2025-02-01 10:00", "MSFT", "BUY");
    const buy_id_2025_2 = try tc.insertTradeReturnId(handle, "2025-03-01 10:00", "AAPL", "BUY");

    // IKBR / AAPL (2024)
    var r1 = api.schema.zp_fifo_snapshot.zero();
    tc.setBufZ(r1.broker[0..], "IKBR");
    r1.tax_year = 2024;
    tc.setBufZ(r1.ticker[0..], "AAPL");
    r1.acq_trade_id = buy_id_2024;
    tc.setBufZ(r1.acq_datetime[0..], "2024-01-01 10:00");
    r1.qty_remaining = 1.0;
    r1.cost_per_share_eur = 90.0;
    r1.acq_commission_eur = std.math.nan(f64);
    tc.setBufZ(r1.country[0..], "US");

    // DEGIRO / MSFT (2025)
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

    // DEGIRO / AAPL (2025) - ensures broker-only isn’t enough
    var r3 = api.schema.zp_fifo_snapshot.zero();
    tc.setBufZ(r3.broker[0..], "DEGIRO");
    r3.tax_year = 2025;
    tc.setBufZ(r3.ticker[0..], "AAPL");
    r3.acq_trade_id = buy_id_2025_2;
    tc.setBufZ(r3.acq_datetime[0..], "2025-03-01 10:00");
    r3.qty_remaining = 3.0;
    r3.cost_per_share_eur = 100.0;
    r3.acq_commission_eur = 0.0;
    tc.setBufZ(r3.country[0..], "US");

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_insert_fifo_snapshot(handle, &r1));
    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_insert_fifo_snapshot(handle, &r2));
    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_insert_fifo_snapshot(handle, &r3));

    // Query: up to 2025, broker=DEGIRO, ticker=MSFT => should return ONLY r2
    var out: [8]api.schema.zp_fifo_snapshot = undefined;
    var count: usize = 0;

    try std.testing.expectEqual(
        api.helper.ErrorCode.ok,
        api.zp_sqlite_read_fifo_snapshot_by_year_broker_ticker(handle, 2025, "DEGIRO", "MSFT", &out, out.len, &count),
    );
    try std.testing.expectEqual(@as(usize, 1), count);
    try std.testing.expectEqualStrings("DEGIRO", tc.zstr(out[0].broker[0..]));
    try std.testing.expectEqualStrings("MSFT", tc.zstr(out[0].ticker[0..]));
    try std.testing.expectEqual(@as(u32, 2025), out[0].tax_year);

    // Query: up to 2024, broker=DEGIRO, ticker=MSFT => should be 0 (row is 2025)
    var out2: [8]api.schema.zp_fifo_snapshot = undefined;
    var count2: usize = 0;

    try std.testing.expectEqual(
        api.helper.ErrorCode.ok,
        api.zp_sqlite_read_fifo_snapshot_by_year_broker_ticker(handle, 2024, "DEGIRO", "MSFT", &out2, out2.len, &count2),
    );
    try std.testing.expectEqual(@as(usize, 0), count2);
}

test "fifo_snapshot: delete by lot_id" {
    tc.vprint("fifo_snapshot: delete by lot_id");

    const handle = try tc.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    try tc.ensureFifoTables(handle);

    const buy_id = try tc.insertTradeReturnId(handle, "2024-01-01 10:00", "AAPL", "BUY");

    var r1 = api.schema.zp_fifo_snapshot.zero();
    tc.setBufZ(r1.broker[0..], "IKBR");
    r1.tax_year = 2024;
    tc.setBufZ(r1.ticker[0..], "AAPL");
    r1.acq_trade_id = buy_id;
    tc.setBufZ(r1.acq_datetime[0..], "2024-01-01 10:00");
    r1.qty_remaining = 1.0;
    r1.cost_per_share_eur = 90.0;
    r1.acq_commission_eur = 0.0;
    tc.setBufZ(r1.country[0..], "US");

    var r2 = api.schema.zp_fifo_snapshot.zero();
    tc.setBufZ(r2.broker[0..], "IKBR");
    r2.tax_year = 2024;
    tc.setBufZ(r2.ticker[0..], "AAPL");
    r2.acq_trade_id = buy_id;
    tc.setBufZ(r2.acq_datetime[0..], "2024-02-01 10:00");
    r2.qty_remaining = 2.0;
    r2.cost_per_share_eur = 95.0;
    r2.acq_commission_eur = 0.0;
    tc.setBufZ(r2.country[0..], "US");

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_insert_fifo_snapshot(handle, &r1));
    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_insert_fifo_snapshot(handle, &r2));

    // Read all -> get a real lot_id assigned by DB
    var out: [8]api.schema.zp_fifo_snapshot = undefined;
    var count: usize = 0;
    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_snapshot_all(handle, &out, out.len, &count));
    try std.testing.expectEqual(@as(usize, 2), count);

    const delete_id: u32 = out[0].lot_id;

    // Delete
    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_delete_fifo_snapshot_by_lot_id(handle, delete_id));

    // Verify count decremented
    var out2: [8]api.schema.zp_fifo_snapshot = undefined;
    var count2: usize = 0;
    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_snapshot_all(handle, &out2, out2.len, &count2));
    try std.testing.expectEqual(@as(usize, 1), count2);

    // Deleting again should fail (not found)
    try std.testing.expectEqual(api.helper.ErrorCode.execution_fail, api.zp_sqlite_delete_fifo_snapshot_by_lot_id(handle, delete_id));
}

test "fifo_snapshot: update qty_remaining by lot_id" {
    tc.vprint("fifo_snapshot: update qty_remaining by lot_id");

    const handle = try tc.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    try tc.ensureFifoTables(handle);

    const buy_id = try tc.insertTradeReturnId(handle, "2024-01-01 10:00", "AAPL", "BUY");

    var r = api.schema.zp_fifo_snapshot.zero();
    tc.setBufZ(r.broker[0..], "IKBR");
    r.tax_year = 2024;
    tc.setBufZ(r.ticker[0..], "AAPL");
    r.acq_trade_id = buy_id;
    tc.setBufZ(r.acq_datetime[0..], "2024-01-01 10:00");
    r.qty_remaining = 10.0;
    r.cost_per_share_eur = 100.0;
    r.acq_commission_eur = 0.0;
    tc.setBufZ(r.country[0..], "US");

    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_insert_fifo_snapshot(handle, &r));

    // Read back to obtain lot_id
    var out: [8]api.schema.zp_fifo_snapshot = undefined;
    var count: usize = 0;
    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_snapshot_all(handle, &out, out.len, &count));
    try std.testing.expectEqual(@as(usize, 1), count);

    const lot_id: u32 = out[0].lot_id;

    // Update qty_remaining
    try std.testing.expectEqual(
        api.helper.ErrorCode.ok,
        api.zp_sqlite_update_fifo_snapshot_qty_remaining_by_lot_id(handle, lot_id, 3.5),
    );

    // Verify
    var out2: [8]api.schema.zp_fifo_snapshot = undefined;
    var count2: usize = 0;
    try std.testing.expectEqual(api.helper.ErrorCode.ok, api.zp_sqlite_read_fifo_snapshot_all(handle, &out2, out2.len, &count2));
    try std.testing.expectEqual(@as(usize, 1), count2);
    try std.testing.expectApproxEqAbs(@as(f64, 3.5), out2[0].qty_remaining, 1e-12);

    // Update non-existent lot_id should fail
    try std.testing.expectEqual(
        api.helper.ErrorCode.execution_fail,
        api.zp_sqlite_update_fifo_snapshot_qty_remaining_by_lot_id(handle, lot_id + 9999, 1.0),
    );
}
