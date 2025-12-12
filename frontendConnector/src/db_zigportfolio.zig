const std = @import("std");

const dbport = @import("db.zig");

/// Bindings to the zigPortfolio C API.
pub const zp = @cImport({
    @cInclude("zigPortfolio.h");
});

/// Errors that can occur when opening the database.
pub const DbOpenError = error{
    DbOpenFailed,
    OutOfMemory,
};

const Ctx = struct {
    alloc: std.mem.Allocator,
    handle: zp.zp_db_handle,
};

fn vDeinit(ctx_ptr: *anyopaque) void {
    const ctx: *Ctx = @ptrCast(@alignCast(ctx_ptr));

    const rc = zp.zp_sqlite_close(ctx.handle);
    if (rc != zp.ZP_ERROR_OK) {
        std.debug.print("⚠️ Failed to close DB (err={d})\n", .{rc});
    } else {
        std.debug.print("✅ Closed DB\n", .{});
    }

    ctx.alloc.destroy(ctx);
}

fn vAddMockRecord(ctx_ptr: *anyopaque) anyerror!void {
    // Current zigPortfolio C ABI in this workspace does not expose an exec/insert
    // primitive yet, so keep this as a non-mutating placeholder.
    _ = ctx_ptr;
    std.debug.print("🧪 addMockRecord: would insert a record into the portfolio DB here.\n", .{});
}

/// Open the portfolio database at an explicit path.
pub fn openDb(alloc: std.mem.Allocator, path: []const u8) DbOpenError!dbport.Db {
    const ctx = alloc.create(Ctx) catch return error.OutOfMemory;
    errdefer alloc.destroy(ctx);

    ctx.alloc = alloc;
    ctx.handle = 0;

    // C ABI expects a null-terminated string.
    const zpath = alloc.dupeZ(u8, path) catch return error.OutOfMemory;
    defer alloc.free(zpath);

    const rc = zp.zp_sqlite_open(zpath, &ctx.handle);
    if (rc != zp.ZP_ERROR_OK) {
        std.debug.print("❌ Failed to open DB \"{s}\" (err={d})\n", .{ path, rc });
        return error.DbOpenFailed;
    }

    std.debug.print("✅ Opened DB. Handle = 0x{x}\n", .{ctx.handle});

    return .{
        .ctx = ctx,
        .vtable = &.{
            .deinit = vDeinit,
            .addMockRecord = vAddMockRecord,
        },
    };
}

/// Open the default portfolio database.
///
/// This mirrors the previous behavior: "../db/portfolio.db" relative to the
/// frontendConnector working directory.
pub fn openDefaultDb(alloc: std.mem.Allocator) DbOpenError!dbport.Db {
    return openDb(alloc, "../db/portfolio.db");
}

/// Log the zigPortfolio library version using the C ABI helpers.
pub fn logLibraryVersion() void {
    const major = zp.zp_version_major();
    const minor = zp.zp_version_minor();
    const patch = zp.zp_version_patch();

    var buf: [32]u8 = undefined;
    const written = zp.zp_version_string(&buf, buf.len);

    std.debug.print("zigPortfolio version: {d}.{d}.{d} ({s})\n", .{
        major,
        minor,
        patch,
        buf[0..written],
    });
}
