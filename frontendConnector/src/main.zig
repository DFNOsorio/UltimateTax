const std = @import("std");

pub const zp = @cImport({
    @cInclude("zigPortfolio.h");
});

pub fn main() !void {

    // Version
    const major = zp.zp_version_major();
    const minor = zp.zp_version_minor();
    const patch = zp.zp_version_patch();
    std.debug.print("Version: {d}.{d}.{d}\n", .{ major, minor, patch });

    // DB handle coming from the header (uintptr_t)
    var handle: zp.zp_db_handle = 0;
    const db_path = "../db/portfolio.db";

    const rc_open = zp.zp_sqlite_open(db_path, &handle);
    if (rc_open != zp.ZP_ERROR_OK) {
        std.debug.print("❌ Failed to open DB (err={d})\n", .{rc_open});
        return;
    }

    std.debug.print("✅ Opened DB. Handle = 0x{x}\n", .{handle});

    const rc_close = zp.zp_sqlite_close(handle);
    if (rc_close != zp.ZP_ERROR_OK) {
        std.debug.print("⚠️ Failed to close DB (err={d})\n", .{rc_close});
    } else {
        std.debug.print("✅ Closed DB successfully.\n", .{});
    }
}
