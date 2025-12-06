const std = @import("std");
const helper = @import("helper.zig");

/// Simple exported hello world C ABI function
pub fn sqlite_hello_impl() !void {
    std.debug.print("Hello from sqliteConnector!\n", .{});
}
