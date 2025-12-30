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

    // Install header
    const install_header = b.addInstallFile(
        b.path("include/zigPortfolio.h"),
        "zigPortfolio.h",
    );
    b.getInstallStep().dependOn(&install_header.step);

    // Install the library under zig-out/lib/
    b.installArtifact(lib);

    // -------------------------
    // Unit tests (two variants)
    // -------------------------

    // API module for tests (src/lib.zig), with pkgmeta provided
    const api_mod = b.createModule(.{
        .root_source_file = b.path("src/lib.zig"),
        .target = target,
        .optimize = optimize,
    });
    api_mod.addOptions("pkgmeta", opts);

    // ---- Normal tests (quiet) ----
    const testopts_quiet = b.addOptions();
    testopts_quiet.addOption(bool, "verbose_test_names", false);

    const tests_mod = b.createModule(.{
        .root_source_file = b.path("tests/tests.zig"),
        .target = target,
        .optimize = optimize,
    });
    tests_mod.addImport("api", api_mod);
    tests_mod.addOptions("testopts", testopts_quiet);

    const unit_tests = b.addTest(.{
        .root_module = tests_mod,
    });
    unit_tests.linkSystemLibrary("sqlite3");

    const run_unit_tests = b.addRunArtifact(unit_tests);
    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_unit_tests.step);

    // ---- Verbose tests (prints names) ----
    const testopts_verbose = b.addOptions();
    testopts_verbose.addOption(bool, "verbose_test_names", true);

    const tests_mod_verbose = b.createModule(.{
        .root_source_file = b.path("tests/tests.zig"),
        .target = target,
        .optimize = optimize,
    });
    tests_mod_verbose.addImport("api", api_mod);
    tests_mod_verbose.addOptions("testopts", testopts_verbose);

    const unit_tests_verbose = b.addTest(.{
        .root_module = tests_mod_verbose,
    });
    unit_tests_verbose.linkSystemLibrary("sqlite3");

    // Run emitted test binary directly (avoids the --listen=- IPC runner behavior)
    const run_unit_tests_verbose = std.Build.Step.Run.create(b, "run unit tests (verbose)");
    run_unit_tests_verbose.step.dependOn(&unit_tests_verbose.step);
    run_unit_tests_verbose.addFileArg(unit_tests_verbose.getEmittedBin());
    run_unit_tests_verbose.stdio = .inherit;
    run_unit_tests_verbose.has_side_effects = true;

    const test_verbose_step = b.step("test-verbose", "Run unit tests with verbose output");
    test_verbose_step.dependOn(&run_unit_tests_verbose.step);
}
