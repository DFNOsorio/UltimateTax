const std = @import("std");
const Build = std.Build;

pub fn build(b: *Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Create a module for main.zig
    const exe_mod = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    // Create the executable using that module
    const exe = b.addExecutable(.{
        .name = "frontendConnector",
        .root_module = exe_mod,
    });

    // Tell the linker where to find libzigPortfolio.dylib
    exe.addLibraryPath(.{ .cwd_relative = "../build_output" });
    exe.linkSystemLibrary("zigPortfolio");

    b.installArtifact(exe);
}
