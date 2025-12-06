const std = @import("std");

// C ABI functions from libzigPortfolio
extern fn zp_sqlite_hello() void;
extern fn zp_version_major() c_int;
extern fn zp_version_minor() c_int;
extern fn zp_version_patch() c_int;
extern fn zp_version_string(buf: [*]u8, len: usize) usize;

pub fn main() !void {
    // Call hello from the shared library
    zp_sqlite_hello();

    // Get numeric version
    const major = zp_version_major();
    const minor = zp_version_minor();
    const patch = zp_version_patch();

    std.debug.print("Version numbers: {d}.{d}.{d}\n", .{ major, minor, patch });

    // Get version string
    var buffer: [32]u8 = undefined;
    const written = zp_version_string(&buffer, buffer.len);

    std.debug.print("Version string: {s}\n", .{buffer[0..written]});
}
