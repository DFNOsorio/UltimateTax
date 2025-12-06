const std = @import("std");
const helper = @import("helper.zig");

/// Simple exported hello world C ABI function
export fn zp_sqlite_hello() void {
    std.debug.print("Hello from sqliteConnector!\n", .{});
}
