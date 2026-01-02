const std = @import("std");
const testopts = @import("testopts");

const api = @import("api");
const sqlite = api.sqlite;
const c = sqlite.c;

pub const helper = api.helper;
pub const trade = api.trade;

pub inline fn vprint(comptime msg: []const u8) void {
    if (testopts.verbose_test_names) {
        std.debug.print("RUNNING: {s}\n", .{msg});
    }
}

pub const schema_sql: [:0]const u8 =
    \\PRAGMA foreign_keys = ON;
    \\DROP TABLE IF EXISTS trades;
    \\CREATE TABLE trades (
    \\    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    \\    broker              TEXT NOT NULL DEFAULT 'IKBR',
    \\    trade_datetime      TEXT NOT NULL,
    \\    type                TEXT NOT NULL CHECK (type IN ('BUY', 'SELL')) DEFAULT 'BUY',
    \\    ticker              TEXT NOT NULL,
    \\    quantity            REAL NOT NULL,
    \\    price_per_share     REAL NOT NULL,
    \\    commission          REAL NOT NULL DEFAULT 0.0,
    \\    country             TEXT NOT NULL DEFAULT 'US',
    \\    currency            TEXT NOT NULL DEFAULT 'USD',
    \\    conversion_rate_eur REAL NOT NULL DEFAULT 1.0
    \\);
    \\CREATE INDEX idx_trades_datetime ON trades(trade_datetime);
    \\CREATE INDEX idx_trades_ticker_datetime ON trades(ticker, trade_datetime);
;

pub fn execSql(db: *c.sqlite3, sql: [:0]const u8) !void {
    var errmsg: [*c]u8 = null;
    const errmsg_ptr: [*c][*c]u8 = @ptrCast(&errmsg);

    const rc: c_int = c.sqlite3_exec(db, sql.ptr, null, null, errmsg_ptr);
    defer {
        if (errmsg != null) c.sqlite3_free(errmsg);
    }
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), rc);
}

pub fn openMemDb() !helper.DbHandle {
    const mem: [:0]const u8 = ":memory:";
    var handle: helper.DbHandle = helper.INVALID_DB_HANDLE;
    try std.testing.expectEqual(helper.ErrorCode.ok, api.zp_sqlite_open(mem.ptr, &handle));
    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, schema_sql);
    return handle;
}

pub fn setBufZ(buf: []u8, s: []const u8) void {
    @memset(buf, 0);
    if (buf.len == 0) return;
    const n = @min(s.len, buf.len - 1);
    std.mem.copyForwards(u8, buf[0..n], s[0..n]);
    buf[n] = 0;
}

pub fn countTrades(db: *c.sqlite3) !i64 {
    var stmt: ?*c.sqlite3_stmt = null;
    const sql: [:0]const u8 = "SELECT COUNT(*) FROM trades;";

    var rc: c_int = c.sqlite3_prepare_v2(db, sql.ptr, -1, &stmt, null);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_OK), rc);
    defer _ = c.sqlite3_finalize(stmt.?);

    rc = c.sqlite3_step(stmt.?);
    try std.testing.expectEqual(@as(c_int, c.SQLITE_ROW), rc);

    return @as(i64, c.sqlite3_column_int64(stmt.?, 0));
}

// -----------------------------------------------------------------------------
// Added for FIFO tests (keeps all existing helpers intact)
// -----------------------------------------------------------------------------

pub const fifo_schema_sql: [:0]const u8 =
    \\PRAGMA foreign_keys = ON;
    \\DROP TABLE IF EXISTS fifo_snapshot;
    \\CREATE TABLE fifo_snapshot (
    \\    lot_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    \\    broker              TEXT NOT NULL,
    \\    tax_year            INTEGER NOT NULL,
    \\    ticker              TEXT NOT NULL,
    \\    acq_trade_id        INTEGER NOT NULL,
    \\    acq_datetime        TEXT NOT NULL,
    \\    qty_remaining       REAL NOT NULL CHECK (qty_remaining >= 0.0),
    \\    cost_per_share_eur  REAL NOT NULL CHECK (cost_per_share_eur >= 0.0),
    \\    acq_commission_eur  REAL NOT NULL DEFAULT 0.0 CHECK (acq_commission_eur >= 0.0),
    \\    country             TEXT NOT NULL,
    \\    FOREIGN KEY (acq_trade_id) REFERENCES trades(id)
    \\);
    \\CREATE INDEX IF NOT EXISTS idx_fifo_snapshot_broker_year_ticker
    \\ON fifo_snapshot(broker, tax_year, ticker, acq_datetime, lot_id);
    \\
    \\DROP TABLE IF EXISTS fifo_realized;
    \\CREATE TABLE fifo_realized (
    \\    operation_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    \\    broker              TEXT NOT NULL,
    \\    tax_year            INTEGER NOT NULL,
    \\    ticker              TEXT NOT NULL,
    \\    sell_trade_id       INTEGER NOT NULL,
    \\    buy_trade_id        INTEGER NOT NULL,
    \\    match_seq           INTEGER NOT NULL,
    \\    sell_datetime       TEXT NOT NULL,
    \\    buy_datetime        TEXT NOT NULL,
    \\    qty_matched         REAL NOT NULL CHECK (qty_matched > 0.0),
    \\    proceeds_eur        REAL NOT NULL,
    \\    cost_eur            REAL NOT NULL,
    \\    gain_eur            REAL NOT NULL,
    \\    FOREIGN KEY (sell_trade_id) REFERENCES trades(id),
    \\    FOREIGN KEY (buy_trade_id)  REFERENCES trades(id),
    \\    UNIQUE (sell_trade_id, match_seq)
    \\);
    \\CREATE INDEX IF NOT EXISTS idx_fifo_realized_broker_year_ticker
    \\ON fifo_realized(broker, tax_year, ticker, sell_datetime, operation_id);
;

pub fn ensureFifoTables(handle: helper.DbHandle) !void {
    const db: *c.sqlite3 = @ptrFromInt(handle);
    try execSql(db, fifo_schema_sql);
}

pub fn lastInsertRowId(handle: helper.DbHandle) u32 {
    const db: *c.sqlite3 = @ptrFromInt(handle);
    const id64 = c.sqlite3_last_insert_rowid(db);
    return @as(u32, @intCast(id64));
}

pub fn insertTradeReturnId(
    handle: helper.DbHandle,
    trade_datetime: [:0]const u8,
    ticker: [:0]const u8,
    trade_type: [:0]const u8,
) !u32 {
    // Use defaults by passing nulls/NaNs where applicable (matching your insert tests style)
    try std.testing.expectEqual(
        helper.ErrorCode.ok,
        api.zp_sqlite_insert_trade(
            handle,
            trade_datetime.ptr,
            ticker.ptr,
            1.0,
            100.0,
            null,
            trade_type.ptr,
            std.math.nan(f64),
            null,
            null,
            std.math.nan(f64),
        ),
    );
    return lastInsertRowId(handle);
}

pub fn zstr(buf: []const u8) []const u8 {
    return std.mem.sliceTo(buf, 0);
}
