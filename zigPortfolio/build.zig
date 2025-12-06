const std = @import("std");
const Build = std.Build;

// Embed the manifest here, next to build.zig
const build_zon_text = @embedFile("build.zig.zon");

pub fn build(b: *Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib_mod = b.createModule(.{
        .root_source_file = b.path("src/lib.zig"),
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addLibrary(.{
        .name = "zigPortfolio",
        .root_module = lib_mod,
        .linkage = .dynamic, // builds a .dylib on macOS
    });

    // Create an options module and attach the manifest contents
    const opts = b.addOptions();
    opts.addOption([]const u8, "build_zon", build_zon_text);

    // This will be available in lib.zig as: const pkgmeta = @import("pkgmeta");
    lib.root_module.addOptions("pkgmeta", opts);

    b.installArtifact(lib);
}
