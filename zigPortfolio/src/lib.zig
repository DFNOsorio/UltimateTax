const std = @import("std");

pub const sqlite = @import("sqliteConnector.zig");
pub const helper = @import("helper.zig");

pub const trade_mod = @import("trade.zig");
pub const zp_trade = trade_mod.zp_trade;

// Options module passed from build.zig
const pkgmeta = @import("pkgmeta");

const DbHandle = helper.DbHandle;

// C ABI: int zp_sqlite_open(const char *path, zp_db_handle *out_handle);
pub export fn zp_sqlite_open(
    path: [*:0]const u8,
    out_handle: *DbHandle,
) helper.ErrorCode {
    return sqlite.sqlite_open_handle_impl(path, out_handle);
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

    return sqlite.sqlite_insert_trade(
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
    trade: ?*const zp_trade,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (trade == null) return .invalid_argument;

    const t = trade.?;

    // Required fields
    if (t.trade_datetime == null or t.ticker == null) return .invalid_argument;

    // Reuse your existing code path
    return sqlite.sqlite_insert_trade(
        handle,
        t.trade_datetime.?,
        t.ticker.?,
        t.quantity,
        t.price_per_share,
        t.broker,
        t.type,
        t.commission,
        t.country,
        t.currency,
        t.conversion_rate_eur,
    );
}

pub export fn zp_sqlite_read_trade_by_id(
    handle: DbHandle,
    id: u32,
    out_trade: ?*zp_trade,
) helper.ErrorCode {
    if (out_trade == null) return .invalid_argument;
    return sqlite.sqlite_read_trade_by_id(handle, id, out_trade.?);
}

pub export fn zp_trade_free(trade: ?*zp_trade) void {
    if (trade == null) return;
    sqlite.trade_free(trade.?);
}

// C ABI: int zp_sqlite_close(zp_db_handle handle);
pub export fn zp_sqlite_close(handle: DbHandle) helper.ErrorCode {
    return sqlite.sqlite_close_handle_impl(handle);
}

pub const Version = struct {
    major: u8 = 0,
    minor: u8 = 0,
    patch: u8 = 0,

    pub fn toInt(self: Version) u32 {
        return (@as(u32, self.major) << 16) | (@as(u32, self.minor) << 8) | (@as(u32, self.patch));
    }

    pub fn format(self: Version, writer: anytype) !void {
        try writer.print("{d}.{d}.{d}", .{
            self.major,
            self.minor,
            self.patch,
        });
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

        if (after_eq.len < 2 or after_eq[0] != '"' or after_eq[after_eq.len - 1] != '"')
            break;

        const ver_str = after_eq[1 .. after_eq.len - 1]; // strip quotes

        var parts = std.mem.splitScalar(u8, ver_str, '.');

        if (parts.next()) |a|
            major = std.fmt.parseUnsigned(u8, a, 10) catch 0;

        if (parts.next()) |b|
            minor = std.fmt.parseUnsigned(u8, b, 10) catch 0;

        if (parts.next()) |c|
            patch = std.fmt.parseUnsigned(u8, c, 10) catch 0;

        break;
    }

    return .{
        .major = major,
        .minor = minor,
        .patch = patch,
    };
}

// Computed once at program init from build.zig.zon contents
const BUILD_VERSION: Version = parseVersionFromZon(pkgmeta.build_zon);

pub fn getVersion() Version {
    return BUILD_VERSION;
}

// C ABI exports – version as integers
export fn zp_version_major() c_int {
    return @as(c_int, BUILD_VERSION.major);
}

export fn zp_version_minor() c_int {
    return @as(c_int, BUILD_VERSION.minor);
}

export fn zp_version_patch() c_int {
    return @as(c_int, BUILD_VERSION.patch);
}

// C ABI export – writes "M.m.p" into caller-provided buffer as a C string.
// Returns number of bytes written (excluding the null terminator).
export fn zp_version_string(buf: [*]u8, buf_len: usize) usize {
    if (buf_len == 0) return 0;

    const slice = std.fmt.bufPrintZ(
        buf[0..buf_len],
        "{d}.{d}.{d}",
        .{ BUILD_VERSION.major, BUILD_VERSION.minor, BUILD_VERSION.patch },
    ) catch return 0;

    // `slice` excludes the trailing '\0'
    return slice.len;
}
