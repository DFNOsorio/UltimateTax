const std = @import("std");

const db_real = @import("db_zigportfolio.zig");
const server = @import("net.zig");

pub fn main() !void {
    const alloc = std.heap.c_allocator;

    var database = try db_real.openDefaultDb(alloc);
    defer database.deinit();

    db_real.logLibraryVersion();

    try server.runServer(&database);
}
