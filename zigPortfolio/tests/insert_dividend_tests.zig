const std = @import("std");
const api = @import("api");
const helper = api.helper;
const sqlite = api.sqlite;
const c = sqlite.c;

const common = @import("test_common.zig");

fn read_dividend_generated(db: *c.sqlite3) !struct { shares: f64, tax_rate: f64 } {
    var stmt: ?*c.sqlite3_stmt = null;
    const sql: [:0]const u8 = "SELECT number_of_shares, tax_rate FROM dividends WHERE dividend_id = 1;";

    var rc: c_int = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), rc);
    defer _ = c.sqlite3_finalize(stmt.?);

    rc = c.sqlite3_step(stmt.?);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_ROW), rc);

    const shares = c.sqlite3_column_double(stmt.?, 0);
    const rate = c.sqlite3_column_double(stmt.?, 1);

    return .{ .shares = shares, .tax_rate = rate };
}

test "insert_dividend_struct: inserts and generated columns compute" {
    common.vprint("insert_dividend_struct: inserts and generated columns compute");

    const handle = try common.openMemDb();
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);

    var d: api.schema.zp_dividend = undefined;
    api.schema.clearDividend(&d);

    common.setBufZ(d.broker[0..], "DEMO");
    common.setBufZ(d.dividend_dt[0..], "2025-12-31 00:00");
    common.setBufZ(d.ticker[0..], "AAA");
    common.setBufZ(d.country[0..], "PT");
    common.setBufZ(d.currency[0..], "EUR");

    d.per_share = 0.50;
    d.total_amount = 10.00;
    d.tax = 2.00;
    d.conversion_rate_eur = 1.0;

    const rc = api.zp_sqlite_insert_dividend_struct(handle, &d);
    try std.testing.expectEqual(helper.ErrorCode.ok, rc);

    const gen = try read_dividend_generated(db);
    try std.testing.expectApproxEqAbs(@as(f64, 20.0), gen.shares, 1e-12);
    try std.testing.expectApproxEqAbs(@as(f64, 20.0), gen.tax_rate, 1e-12);
}

test "insert_dividend: invalid_argument when required pointers are NULL" {
    common.vprint("insert_dividend: invalid_argument when required pointers are NULL");

    const mem: [:0]const u8 = ":memory:";
    var handle: helper.DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    defer _ = api.zp_sqlite_close(handle);

    const db: *c.sqlite3 = @ptrFromInt(handle);
    try common.execSql(db, common.schema_sql);

    try std.testing.expectEqual(
        helper.ErrorCode.invalid_argument,
        api.zp_sqlite_insert_dividend(
            handle,
            null,
            "2025-12-31 00:00".ptr,
            "AAA".ptr,
            "PT".ptr,
            0.5,
            10.0,
            2.0,
            "EUR".ptr,
            1.0,
        ),
    );
}
