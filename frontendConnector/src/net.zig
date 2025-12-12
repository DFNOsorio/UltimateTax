const std = @import("std");

const Address = std.net.Address;
const Stream = std.net.Stream;
const posix = std.posix;
const mem = std.mem;

const db = @import("db.zig");
const http = @import("http.zig");

/// C signal API for handling Ctrl+C.
const c = @cImport({
    @cInclude("signal.h");
});

/// Global shutdown flag, flipped by the SIGINT handler.
var g_should_quit: bool = false;

/// SIGINT (Ctrl+C) handler. Only sets a flag; main loop checks it.
///
/// Must match the C signal handler type: fn(c_int) callconv(.c) void.
fn handle_sigint(sig: c_int) callconv(.c) void {
    _ = sig;
    g_should_quit = true;
}

/// Run the TCP server on 127.0.0.1:8080.
///
/// This function owns the listening socket and will close it on return.
/// It delegates per-connection HTTP handling to http.handleConnection.
pub fn runServer(db_handle: db.DbHandle) !void {
    const addr = try Address.parseIpAndPort("127.0.0.1:8080");

    const nonblock: u32 = if (@hasDecl(posix.SOCK, "NONBLOCK")) posix.SOCK.NONBLOCK else 0;
    const sock_flags = posix.SOCK.STREAM | posix.SOCK.CLOEXEC | nonblock;
    const proto: u32 = if (addr.any.family == posix.AF.UNIX) 0 else posix.IPPROTO.TCP;

    const sockfd = try posix.socket(addr.any.family, sock_flags, proto);
    defer posix.close(sockfd);

    // Allow address/port reuse (Zig 0.15.2 uses posix.SO.REUSEADDR)
    try posix.setsockopt(
        sockfd,
        posix.SOL.SOCKET,
        posix.SO.REUSEADDR,
        &mem.toBytes(@as(i32, 1)),
    );

    if (@hasDecl(posix.SO, "REUSEPORT") and addr.any.family != posix.AF.UNIX) {
        try posix.setsockopt(
            sockfd,
            posix.SOL.SOCKET,
            posix.SO.REUSEPORT,
            &mem.toBytes(@as(i32, 1)),
        );
    }

    const socklen = addr.getOsSockLen();
    try posix.bind(sockfd, &addr.any, socklen);
    try posix.listen(sockfd, 128);

    _ = c.signal(c.SIGINT, handle_sigint);

    std.debug.print("🌐 Listening on http://127.0.0.1:8080 (Ctrl+C to stop gracefully)\n", .{});
    std.debug.print("   POST /add will call the mock DB add endpoint.\n", .{});

    while (!g_should_quit) {
        var accepted_addr: Address = undefined;
        var addr_len: posix.socklen_t = @sizeOf(Address);

        const fd = posix.accept(
            sockfd,
            &accepted_addr.any,
            &addr_len,
            posix.SOCK.CLOEXEC,
        ) catch |err| switch (err) {
            error.WouldBlock => continue,
            else => {
                if (g_should_quit) break;
                return err;
            },
        };

        if (g_should_quit) {
            posix.close(fd);
            break;
        }

        var stream = Stream{ .handle = fd };
        std.debug.print("🔌 New connection from ", .{});
        printAddress(accepted_addr);
        std.debug.print("\n", .{});

        var buf: [4096]u8 = undefined;
        const n = stream.read(&buf) catch {
            stream.close();
            continue;
        };

        // If your http.handleConnection expects a pointer, pass &stream; otherwise pass stream.
        // Recommended: make it take *Stream to be explicit.
        try http.handleConnection(&stream, db_handle, buf[0..n]);

        stream.close();
    }

    std.debug.print("👋 Ctrl+C detected, shutting down gracefully.\n", .{});
}

fn printAddress(addr: std.net.Address) void {
    switch (addr.any.family) {
        std.posix.AF.INET => {
            const a = addr.in.sa.addr; // u32 in network byte order
            const b0: u8 = @truncate(a >> 24);
            const b1: u8 = @truncate(a >> 16);
            const b2: u8 = @truncate(a >> 8);
            const b3: u8 = @truncate(a);

            const port = std.mem.bigToNative(u16, addr.in.sa.port);
            std.debug.print("{d}.{d}.{d}.{d}:{d}", .{ b0, b1, b2, b3, port });
        },
        std.posix.AF.INET6 => {
            // For now, keep it simple; IPv6 pretty-print can be added next.
            const port = std.mem.bigToNative(u16, addr.in6.sa.port);
            std.debug.print("[IPv6]:{d}", .{port});
        },
        else => {
            std.debug.print("[family={d}]", .{addr.any.family});
        },
    }
}
