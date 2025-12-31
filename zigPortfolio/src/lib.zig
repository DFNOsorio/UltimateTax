const std = @import("std");

pub const sqlite = @import("sqliteConnector.zig");
pub const helper = @import("helper.zig");
pub const trade = @import("trade.zig");

// Options module passed from build.zig
const pkgmeta = @import("pkgmeta");

const DbHandle = helper.DbHandle;

// Re-export for external users (and tests)
pub const zp_trade = trade.zp_trade;

// C ABI: int zp_sqlite_open(const char *path, zp_db_handle *out_handle);
pub export fn zp_sqlite_open(
    path: [*:0]const u8,
    out_handle: *DbHandle,
) helper.ErrorCode {
    return sqlite.sqlite_open_handle_impl(path, out_handle);
}

/// C ABI: scalar-args insert (kept for convenience/backward compatibility)
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

/// C ABI: struct-based insert (uses zp_trade inline buffers)
pub export fn zp_sqlite_insert_trade_struct(
    handle: DbHandle,
    t: ?*const trade.zp_trade,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (t == null) return .invalid_argument;

    // Required inline strings must be non-empty
    if (t.?.trade_datetime[0] == 0 or t.?.ticker[0] == 0) return .invalid_argument;

    return sqlite.sqlite_insert_trade_struct(handle, t.?);
}

/// C ABI: read a trade by id into a caller-provided zp_trade.
pub export fn zp_sqlite_read_trade_by_id(
    handle: DbHandle,
    id: u32,
    out_trade: ?*trade.zp_trade,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_trade == null) return .invalid_argument;

    return sqlite.sqlite_read_trade_by_id(handle, id, out_trade.?);
}

/// C ABI: read trades for a year into caller-provided array.
pub export fn zp_sqlite_read_trades_by_year(
    handle: DbHandle,
    year: u32,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: ?*usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_trades == null or out_count == null) return .invalid_argument;

    return sqlite.sqlite_read_trades_by_year(handle, year, out_trades.?, out_cap, out_count.?);
}

pub export fn zp_sqlite_read_trades_by_broker(
    handle: DbHandle,
    broker: ?[*:0]const u8,
    out_trades: ?[*]trade.zp_trade,
    capacity: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (out_count.* != out_count.*) {} // no-op; keeps some linters quiet (optional)

    out_count.* = 0;

    if (broker == null) return .invalid_argument;
    if (broker.?[0] == 0) return .invalid_argument;

    if (capacity == 0) return .ok;
    if (out_trades == null) return .invalid_argument;

    return sqlite.sqlite_read_trades_by_broker(
        handle,
        broker.?,
        out_trades.?,
        capacity,
        out_count,
    );
}

pub export fn zp_sqlite_read_trades_by_year_and_broker(
    handle: DbHandle,
    year: u32,
    broker: ?[*:0]const u8,
    out_trades: ?[*]trade.zp_trade,
    capacity: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    out_count.* = 0;

    if (broker == null) return .invalid_argument;
    if (broker.?[0] == 0) return .invalid_argument;

    if (capacity == 0) return .ok;
    if (out_trades == null) return .invalid_argument;

    return sqlite.sqlite_read_trades_by_year_and_broker(
        handle,
        year,
        broker.?,
        out_trades.?,
        capacity,
        out_count,
    );
}

pub export fn zp_sqlite_read_all_trades(
    handle: DbHandle,
    out_trades: ?[*]trade.zp_trade,
    out_cap: usize,
    out_count: *usize,
) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;

    // allow caller to query count with cap=0
    if (out_cap == 0) {
        out_count.* = 0;
        return .ok;
    }

    if (out_trades == null) return .invalid_argument;

    return sqlite.sqlite_read_all_trades(handle, out_trades.?, out_cap, out_count);
}

// C ABI: int zp_sqlite_close(zp_db_handle handle);
pub export fn zp_sqlite_close(handle: DbHandle) helper.ErrorCode {
    return sqlite.sqlite_close_handle_impl(handle);
}

// ---------------- Version parsing (unchanged) ----------------

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

        const eq_pos = std.mem.indexOfScalar(u8, line, '=') orelse break;
        const after_eq = std.mem.trim(u8, line[eq_pos + 1 ..], " \t,");

        if (after_eq.len < 2 or after_eq[0] != '"' or after_eq[after_eq.len - 1] != '"')
            break;

        const ver_str = after_eq[1 .. after_eq.len - 1];

        var parts = std.mem.splitScalar(u8, ver_str, '.');

        if (parts.next()) |a| major = std.fmt.parseUnsigned(u8, a, 10) catch 0;
        if (parts.next()) |b| minor = std.fmt.parseUnsigned(u8, b, 10) catch 0;
        if (parts.next()) |c| patch = std.fmt.parseUnsigned(u8, c, 10) catch 0;

        break;
    }

    return .{ .major = major, .minor = minor, .patch = patch };
}

const BUILD_VERSION: Version = parseVersionFromZon(pkgmeta.build_zon);

pub fn getVersion() Version {
    return BUILD_VERSION;
}

export fn zp_version_major() c_int {
    return @as(c_int, BUILD_VERSION.major);
}

export fn zp_version_minor() c_int {
    return @as(c_int, BUILD_VERSION.minor);
}

export fn zp_version_patch() c_int {
    return @as(c_int, BUILD_VERSION.patch);
}

export fn zp_version_string(buf: [*]u8, buf_len: usize) usize {
    if (buf_len == 0) return 0;

    const slice = std.fmt.bufPrintZ(
        buf[0..buf_len],
        "{d}.{d}.{d}",
        .{ BUILD_VERSION.major, BUILD_VERSION.minor, BUILD_VERSION.patch },
    ) catch return 0;

    return slice.len;
}
