const std = @import("std");
const helper = @import("helper.zig");

pub const c = @cImport({
    @cInclude("sqlite3.h");
});

const DbHandle = helper.DbHandle;

pub fn sqlite_hello_impl() void {
    std.debug.print("Hello from sqliteConnector!\n", .{});
}

/// Open a DB and return an opaque handle (pointer-as-handle internally).
pub fn sqlite_open_handle_impl(path: [*:0]const u8, out_handle: *DbHandle) helper.ErrorCode {
    // Precondition: path and out_handle are non-null (C contract)

    var db: ?*c.sqlite3 = null;
    const rc = c.sqlite3_open(path, &db);
    if (rc != c.SQLITE_OK or db == null) {
        if (db != null) {
            _ = c.sqlite3_close(db.?);
        }
        return .open_fail;
    }

    // Convert pointer to integer handle
    out_handle.* = @intFromPtr(db.?);
    return .ok;
}

/// Close a DB given an opaque handle.
pub fn sqlite_close_handle_impl(handle: DbHandle) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) {
        return .invalid_argument;
    }

    // Zig 0.15.2: @ptrFromInt takes ONE argument, type comes from context
    const db_ptr: *c.sqlite3 = @ptrFromInt(handle);

    const rc = c.sqlite3_close(db_ptr);
    if (rc != c.SQLITE_OK) {
        return .close_fail;
    }

    return .ok;
}
