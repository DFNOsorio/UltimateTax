const std = @import("std");
const Build = std.Build;

pub fn build(b: *Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe_mod = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "frontendConnector",
        .root_module = exe_mod,
    });

    // For linking libzigPortfolio.dylib
    exe.addLibraryPath(.{ .cwd_relative = "../build_output" });

    // For @cImport("zigPortfolio.h")
    exe.addIncludePath(.{ .cwd_relative = "../build_output" });

    exe.linkSystemLibrary("zigPortfolio");

    b.installArtifact(exe);
}
