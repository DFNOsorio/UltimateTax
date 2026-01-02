const std = @import("std");
const helper = @import("helper.zig");
const trade = @import("trade.zig");

const insert = @import("insertTrades.zig");
const read = @import("readTrades.zig");
const meta = @import("sqliteMeta.zig");

// Expose sqlite3 C API for tests: tests do `const c = api.sqlite.c;`
pub const c = @cImport({
    @cInclude("sqlite3.h");
});

// Options module passed from build.zig
const pkgmeta = @import("pkgmeta");

const DbHandle = helper.DbHandle;

// ------------------------------------------------------------
// Internal helpers (handle open/close)
// ------------------------------------------------------------

fn sqlite_open_handle_impl(path: [*:0]const u8, out_handle: *DbHandle) helper.ErrorCode {
    var db: ?*c.sqlite3 = null;
    const rc = c.sqlite3_open(path, &db);
    if (rc != c.SQLITE_OK or db == null) {
        if (db != null) _ = c.sqlite3_close(db.?);
        return .open_fail;
    }

    out_handle.* = @intFromPtr(db.?);
    return .ok;
}

fn sqlite_close_handle_impl(handle: DbHandle) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);
    const rc = c.sqlite3_close(db_ptr);
    if (rc != c.SQLITE_OK) return .close_fail;

    return .ok;
}

// ------------------------------------------------------------
// C ABI (moved from old lib.zig)
// ------------------------------------------------------------

// C ABI: zp_error_code zp_sqlite_open(const char *path, zp_db_handle *out_handle);
pub export fn zp_sqlite_open(
    path: [*:0]const u8,
    out_handle: *DbHandle,
) helper.ErrorCode {
    return sqlite_open_handle_impl(path, out_handle);
}

pub export fn zp_sqlite_insert_trade(
    handle: DbHandle,
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
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (trade_datetime == null or ticker == null) return .invalid_argument;

    return insert.sqlite_insert_trade(
        handle,
        trade_datetime.?,
        ticker.?,
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
    handle: DbHandle,
    t: ?*const trade.zp_trade,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (t == null) return .invalid_argument;

    return insert.sqlite_insert_trade_struct(handle, t.?);
}

pub export fn zp_sqlite_read_trade_by_id(
    handle: DbHandle,
    id: u32,
    out_trade: ?*trade.zp_trade,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_trade == null) return .invalid_argument;

    return read.sqlite_read_trade_by_id(handle, id, out_trade.?);
}

pub export fn zp_sqlite_read_trades_by_year(
    handle: DbHandle,
    year: u32,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return read.sqlite_read_trades_by_year(handle, year, out_trades, out_cap, out_count.?);
}

pub export fn zp_sqlite_read_trades_by_broker(
    handle: DbHandle,
    broker: ?[*:0]const u8,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (broker == null) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return read.sqlite_read_trades_by_broker(handle, broker.?, out_trades, out_cap, out_count.?);
}

pub export fn zp_sqlite_read_trades_by_year_and_broker(
    handle: DbHandle,
    year: u32,
    broker: ?[*:0]const u8,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (broker == null) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return read.sqlite_read_trades_by_year_and_broker(handle, year, broker.?, out_trades, out_cap, out_count.?);
}

pub export fn zp_sqlite_read_all_trades(
    handle: DbHandle,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return read.sqlite_read_all_trades(handle, out_trades, out_cap, out_count.?);
}

pub export fn zp_sqlite_get_unique_brokers(
    handle: DbHandle,
    out_brokers: ?[*]trade.zp_broker_name,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return meta.sqlite_get_unique_brokers(handle, out_brokers, out_cap, out_count.?);
}

pub export fn zp_sqlite_get_unique_years(
    handle: DbHandle,
    out_years: ?[*]u32,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count == null) return .invalid_argument;

    return meta.sqlite_get_unique_years(handle, out_years, out_cap, out_count.?);
}

// C ABI: zp_error_code zp_sqlite_close(zp_db_handle handle);
pub export fn zp_sqlite_close(handle: DbHandle) helper.ErrorCode {
    return sqlite_close_handle_impl(handle);
}

// ------------------------------------------------------------
// Version (moved from old lib.zig, matches header: writes into buffer)
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
