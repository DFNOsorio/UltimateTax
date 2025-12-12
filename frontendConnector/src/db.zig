const std = @import("std");

/// Bindings to the zigPortfolio C API and small helpers for DB work.
pub const zp = @cImport({
    @cInclude("zigPortfolio.h");
});

/// Opaque database handle type coming from the C library.
pub const DbHandle = zp.zp_db_handle;

/// Errors that can occur when opening the database.
pub const DbOpenError = error{
    DbOpenFailed,
};

/// Open the default portfolio database and return a handle.
///
/// Currently this always tries to open "../db/portfolio.db"
/// relative to the frontendConnector working directory.
pub fn openDefaultDb() DbOpenError!DbHandle {
    var handle: DbHandle = 0;
    const path = "../db/portfolio.db";

    const rc = zp.zp_sqlite_open(path, &handle);
    if (rc != zp.ZP_ERROR_OK) {
        std.debug.print("❌ Failed to open DB \"{s}\" (err={d})\n", .{ path, rc });
        return error.DbOpenFailed;
    }

    std.debug.print("✅ Opened DB. Handle = 0x{x}\n", .{handle});
    return handle;
}

/// Close a previously opened database handle.
///
/// Logs a warning if the underlying C call fails.
pub fn closeDb(handle: DbHandle) void {
    const rc = zp.zp_sqlite_close(handle);
    if (rc != zp.ZP_ERROR_OK) {
        std.debug.print("⚠️ Failed to close DB (err={d})\n", .{rc});
    } else {
        std.debug.print("✅ Closed DB\n", .{});
    }
}

/// Log the zigPortfolio library version using the C ABI helpers.
pub fn logLibraryVersion() void {
    const major = zp.zp_version_major();
    const minor = zp.zp_version_minor();
    const patch = zp.zp_version_patch();

    var buf: [32]u8 = undefined;
    const written = zp.zp_version_string(&buf, buf.len);

    std.debug.print("zigPortfolio version: {d}.{d}.{d} ({s})\n", .{
        major,           minor, patch,
        buf[0..written],
    });
}

/// Mock “add record” operation.
///
/// For now this only logs a message. Later this will execute
/// a real INSERT using the zigPortfolio SQLite API.
pub fn mockAddRecord(handle: DbHandle) !void {
    _ = handle;
    std.debug.print("🧪 mockAddRecord: would insert a record into the portfolio DB here.\n", .{});
}
