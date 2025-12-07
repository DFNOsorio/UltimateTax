const std = @import("std");
const Build = std.Build;

// Read build.zig.zon for version info
const build_zon_text = @embedFile("build.zig.zon");

pub fn build(b: *Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Root module is src/lib.zig
    const lib_mod = b.createModule(.{
        .root_source_file = b.path("src/lib.zig"),
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addLibrary(.{
        .name = "zigPortfolio",
        .root_module = lib_mod,
        .linkage = .dynamic, // produces libzigPortfolio.dylib on macOS
    });

    // Link system SQLite
    lib.linkSystemLibrary("sqlite3");

    // Options module -> pkgmeta (for your version parsing)
    const opts = b.addOptions();
    opts.addOption([]const u8, "build_zon", build_zon_text);
    lib.root_module.addOptions("pkgmeta", opts);

    const install_header = b.addInstallFile(
        b.path("include/zigPortfolio.h"),
        "zigPortfolio.h",
    );
    b.getInstallStep().dependOn(&install_header.step);

    // Install the library under zigPortfolio/zig-out/lib/
    b.installArtifact(lib);
}
