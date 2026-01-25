const std = @import("std");

pub const sqlite = @import("sqliteConnector.zig");
pub const meta = @import("sqliteMeta.zig");
pub const helper = @import("helper.zig");
pub const schema = @import("schemaStructs.zig");

// Options module passed from build.zig
const pkgmeta = @import("pkgmeta");

// ------------------------------------------------------------
// C ABI exports (rooted here so nm always shows them)
// ------------------------------------------------------------

// Handle management
pub export fn zp_sqlite_open(path: [*:0]const u8, out_handle: *helper.DbHandle) helper.ErrorCode {
    return sqlite.zp_sqlite_open(path, out_handle);
}

pub export fn zp_sqlite_close(handle: helper.DbHandle) helper.ErrorCode {
    return sqlite.zp_sqlite_close(handle);
}

// Trades: insert/read
pub export fn zp_sqlite_insert_trade(
    handle: helper.DbHandle,
    trade_datetime: ?[*:0]const u8,
    ticker: ?[*:0]const u8,
    quantity: f64,
    price_per_share: f64,
    broker: ?[*:0]const u8,
    trade_type: ?[*:0]const u8,
    commission: f64,
    country: ?[*:0]const u8,
    currency: ?[*:0]const u8,
    conversion_rate_eur: f64,
) helper.ErrorCode {
    return sqlite.zp_sqlite_insert_trade(
        handle,
        trade_datetime,
        ticker,
        quantity,
        price_per_share,
        broker,
        trade_type,
        commission,
        country,
        currency,
        conversion_rate_eur,
    );
}

pub export fn zp_sqlite_insert_trade_struct(
    handle: helper.DbHandle,
    t: ?*const schema.zp_trade,
) helper.ErrorCode {
    return sqlite.zp_sqlite_insert_trade_struct(handle, t);
}

pub export fn zp_sqlite_read_trade_by_id(
    handle: helper.DbHandle,
    id: u32,
    out_trade: ?*schema.zp_trade,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_trade_by_id(handle, id, out_trade);
}

pub export fn zp_sqlite_read_trades_by_year(
    handle: helper.DbHandle,
    year: u32,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_trades_by_year(handle, year, out_trades, out_cap, out_count);
}

pub export fn zp_sqlite_read_trades_by_broker(
    handle: helper.DbHandle,
    broker: ?[*:0]const u8,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_trades_by_broker(handle, broker, out_trades, out_cap, out_count);
}

pub export fn zp_sqlite_read_trades_by_year_and_broker(
    handle: helper.DbHandle,
    year: u32,
    broker: ?[*:0]const u8,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_trades_by_year_and_broker(handle, year, broker, out_trades, out_cap, out_count);
}

pub export fn zp_sqlite_read_buy_trades_by_year(
    handle: helper.DbHandle,
    year: u32,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_buy_trades_by_year(handle, year, out_trades, out_cap, out_count);
}

pub export fn zp_sqlite_read_sell_trades_by_year(
    handle: helper.DbHandle,
    year: u32,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_sell_trades_by_year(handle, year, out_trades, out_cap, out_count);
}

pub export fn zp_sqlite_read_all_trades(
    handle: helper.DbHandle,
    out_trades: ?[*]schema.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_all_trades(handle, out_trades, out_cap, out_count);
}

// Meta: uniques
pub export fn zp_sqlite_get_unique_brokers(
    handle: helper.DbHandle,
    out_brokers: ?[*]schema.zp_broker_name,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_get_unique_brokers(handle, out_brokers, out_cap, out_count);
}

pub export fn zp_sqlite_get_unique_years(
    handle: helper.DbHandle,
    out_years: ?[*]u32,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_get_unique_years(handle, out_years, out_cap, out_count);
}

// FIFO snapshot
pub export fn zp_sqlite_insert_fifo_snapshot(
    handle: helper.DbHandle,
    row: ?*const schema.zp_fifo_snapshot,
) helper.ErrorCode {
    return sqlite.zp_sqlite_insert_fifo_snapshot(handle, row);
}

pub export fn zp_sqlite_read_fifo_snapshot_all(
    handle: helper.DbHandle,
    out_rows: ?[*]schema.zp_fifo_snapshot,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_fifo_snapshot_all(handle, out_rows, out_cap, out_count);
}

pub export fn zp_sqlite_read_fifo_snapshot_by_tax_year(
    handle: helper.DbHandle,
    tax_year: u32,
    out_rows: ?[*]schema.zp_fifo_snapshot,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_fifo_snapshot_by_tax_year(handle, tax_year, out_rows, out_cap, out_count);
}

pub export fn zp_sqlite_read_fifo_snapshot_by_ticker_per_year(
    handle: helper.DbHandle,
    tax_year: u32,
    ticker: ?[*:0]const u8,
    out_rows: ?[*]schema.zp_fifo_snapshot,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_fifo_snapshot_by_ticker_per_year(handle, tax_year, ticker, out_rows, out_cap, out_count);
}

pub export fn zp_sqlite_read_fifo_snapshot_by_broker_per_year(
    handle: helper.DbHandle,
    tax_year: u32,
    broker: ?[*:0]const u8,
    out_rows: ?[*]schema.zp_fifo_snapshot,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_fifo_snapshot_by_broker_per_year(handle, tax_year, broker, out_rows, out_cap, out_count);
}

pub export fn zp_sqlite_read_fifo_snapshot_by_year_broker_ticker(
    handle: helper.DbHandle,
    tax_year: u32,
    broker: [*:0]const u8,
    ticker: [*:0]const u8,
    out_rows: ?[*]schema.zp_fifo_snapshot,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_fifo_snapshot_by_year_broker_ticker(
        handle,
        tax_year,
        broker,
        ticker,
        out_rows,
        out_cap,
        out_count,
    );
}

pub export fn zp_sqlite_delete_fifo_snapshot_by_lot_id(
    handle: helper.DbHandle,
    lot_id: u32,
) helper.ErrorCode {
    return sqlite.zp_sqlite_delete_fifo_snapshot_by_lot_id(handle, lot_id);
}

pub export fn zp_sqlite_update_fifo_snapshot_qty_remaining_by_lot_id(
    handle: helper.DbHandle,
    lot_id: u32,
    qty_remaining: f64,
) helper.ErrorCode {
    return sqlite.zp_sqlite_update_fifo_snapshot_qty_remaining_by_lot_id(
        handle,
        lot_id,
        qty_remaining,
    );
}

// FIFO realized
pub export fn zp_sqlite_insert_fifo_realized(
    handle: helper.DbHandle,
    row: ?*const schema.zp_fifo_realized,
) helper.ErrorCode {
    return sqlite.zp_sqlite_insert_fifo_realized(handle, row);
}

pub export fn zp_sqlite_read_fifo_realized_all(
    handle: helper.DbHandle,
    out_rows: ?[*]schema.zp_fifo_realized,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_fifo_realized_all(handle, out_rows, out_cap, out_count);
}

pub export fn zp_sqlite_read_fifo_realized_by_tax_year(
    handle: helper.DbHandle,
    tax_year: u32,
    out_rows: ?[*]schema.zp_fifo_realized,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_fifo_realized_by_tax_year(handle, tax_year, out_rows, out_cap, out_count);
}

pub export fn zp_sqlite_read_fifo_realized_by_ticker_per_year(
    handle: helper.DbHandle,
    tax_year: u32,
    ticker: ?[*:0]const u8,
    out_rows: ?[*]schema.zp_fifo_realized,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_fifo_realized_by_ticker_per_year(handle, tax_year, ticker, out_rows, out_cap, out_count);
}

pub export fn zp_sqlite_read_fifo_realized_by_broker_per_year(
    handle: helper.DbHandle,
    tax_year: u32,
    broker: ?[*:0]const u8,
    out_rows: ?[*]schema.zp_fifo_realized,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_fifo_realized_by_broker_per_year(handle, tax_year, broker, out_rows, out_cap, out_count);
}

// Year processing (load-only)
pub export fn zp_sqlite_process_year_trades_only(handle: helper.DbHandle, year: u32) helper.ErrorCode {
    return sqlite.zp_sqlite_process_year_trades_only(handle, year);
}

// COUNT(*)
pub const zp_table = sqlite.zp_table;

pub export fn zp_sqlite_count_rows(
    db: helper.DbHandle,
    table: meta.zp_table,
    year: ?*const u32,
    broker: ?[*:0]const u8,
    ticker: ?[*:0]const u8,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_count_rows(db, table, year, broker, ticker, out_count);
}

pub export fn zp_sqlite_count_buy_trades_by_year(
    db: helper.DbHandle,
    year: u32,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_count_buy_trades_by_year(db, year, out_count);
}

pub export fn zp_sqlite_count_sell_trades_by_year(
    db: helper.DbHandle,
    year: u32,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_count_sell_trades_by_year(db, year, out_count);
}

pub export fn zp_sqlite_insert_dividend(
    handle: helper.DbHandle,
    broker: ?[*:0]const u8,
    dividend_dt: ?[*:0]const u8,
    ticker: ?[*:0]const u8,
    country: ?[*:0]const u8,
    per_share: f64,
    total_amount: f64,
    tax: f64,
    currency: ?[*:0]const u8,
    conversion_rate_eur: f64,
) helper.ErrorCode {
    return sqlite.zp_sqlite_insert_dividend(
        handle,
        broker,
        dividend_dt,
        ticker,
        country,
        per_share,
        total_amount,
        tax,
        currency,
        conversion_rate_eur,
    );
}

pub export fn zp_sqlite_insert_dividend_struct(
    handle: helper.DbHandle,
    d: ?*const schema.zp_dividend,
) helper.ErrorCode {
    return sqlite.zp_sqlite_insert_dividend_struct(handle, d);
}

pub export fn zp_sqlite_count_dividends(
    handle: helper.DbHandle,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_count_dividends(handle, out_count);
}

pub export fn zp_sqlite_count_dividends_by_year(
    handle: helper.DbHandle,
    year: u32,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_count_dividends_by_year(handle, year, out_count);
}

pub export fn zp_sqlite_count_dividends_by_country(
    handle: helper.DbHandle,
    country: ?[*:0]const u8,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_count_dividends_by_country(handle, country, out_count);
}

pub export fn zp_sqlite_count_dividends_by_year_and_country(
    handle: helper.DbHandle,
    year: u32,
    country: ?[*:0]const u8,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_count_dividends_by_year_and_country(handle, year, country, out_count);
}

pub export fn zp_sqlite_read_all_dividends(
    handle: helper.DbHandle,
    out_rows: ?[*]schema.zp_dividend,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_all_dividends(handle, out_rows, out_cap, out_count);
}

pub export fn zp_sqlite_read_dividends_by_year(
    handle: helper.DbHandle,
    year: u32,
    out_rows: ?[*]schema.zp_dividend,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_dividends_by_year(handle, year, out_rows, out_cap, out_count);
}

pub export fn zp_sqlite_read_dividends_by_country(
    handle: helper.DbHandle,
    country: ?[*:0]const u8,
    out_rows: ?[*]schema.zp_dividend,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_dividends_by_country(handle, country, out_rows, out_cap, out_count);
}

pub export fn zp_sqlite_read_dividends_by_year_and_country(
    handle: helper.DbHandle,
    year: u32,
    country: ?[*:0]const u8,
    out_rows: ?[*]schema.zp_dividend,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    return sqlite.zp_sqlite_read_dividends_by_year_and_country(handle, year, country, out_rows, out_cap, out_count);
}

// ------------------------------------------------------------
// Versioning (rooted here)
// ------------------------------------------------------------

pub const Version = struct {
    major: u8 = 0,
    minor: u8 = 0,
    patch: u8 = 0,

    pub fn toInt(self: Version) u32 {
        return (@as(u32, self.major) << 16) | (@as(u32, self.minor) << 8) | (@as(u32, self.patch));
    }

    pub fn format(self: Version, writer: anytype) !void {
        try writer.print("{d}.{d}.{d}", .{ self.major, self.minor, self.patch });
    }
};

fn parseVersionFromZon(contents: []const u8) Version {
    var it = std.mem.splitScalar(u8, contents, '\n');

    var major: u8 = 0;
    var minor: u8 = 0;
    var patch: u8 = 0;

    while (it.next()) |raw_line| {
        const line = std.mem.trim(u8, raw_line, " \t\r\n");
        if (!std.mem.startsWith(u8, line, ".version")) continue;

        // Expect: .version = "0.0.1",
        const eq_pos = std.mem.indexOfScalar(u8, line, '=') orelse break;
        const after_eq = std.mem.trim(u8, line[eq_pos + 1 ..], " \t,");

        if (after_eq.len < 2 or after_eq[0] != '"' or after_eq[after_eq.len - 1] != '"') break;

        const ver_str = after_eq[1 .. after_eq.len - 1];
        var parts = std.mem.splitScalar(u8, ver_str, '.');

        if (parts.next()) |a| major = std.fmt.parseUnsigned(u8, a, 10) catch 0;
        if (parts.next()) |b| minor = std.fmt.parseUnsigned(u8, b, 10) catch 0;
        if (parts.next()) |cpart| patch = std.fmt.parseUnsigned(u8, cpart, 10) catch 0;

        break;
    }

    return .{ .major = major, .minor = minor, .patch = patch };
}

const BUILD_VERSION: Version = parseVersionFromZon(pkgmeta.build_zon);

pub fn getVersion() Version {
    return BUILD_VERSION;
}

pub export fn zp_version_major() c_int {
    return @as(c_int, BUILD_VERSION.major);
}

pub export fn zp_version_minor() c_int {
    return @as(c_int, BUILD_VERSION.minor);
}

pub export fn zp_version_patch() c_int {
    return @as(c_int, BUILD_VERSION.patch);
}

// Header expects: size_t zp_version_string(char *buf, size_t buf_len);
pub export fn zp_version_string(buf: [*]u8, buf_len: usize) usize {
    if (buf_len == 0) return 0;

    const slice = std.fmt.bufPrintZ(
        buf[0..buf_len],
        "{d}.{d}.{d}",
        .{ BUILD_VERSION.major, BUILD_VERSION.minor, BUILD_VERSION.patch },
    ) catch return 0;

    return slice.len;
}
