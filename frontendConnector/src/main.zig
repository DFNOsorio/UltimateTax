const std = @import("std");

pub const zp = @cImport({
    @cInclude("zigPortfolio.h");
});

// For Ctrl+C handling
const c = @cImport({
    @cInclude("signal.h");
});

const DbHandle = zp.zp_db_handle;

const Address = std.net.Address;
const Stream = std.net.Stream;
const posix = std.posix;
const mem = std.mem;

// Global flag toggled by SIGINT handler
var g_should_quit: bool = false;

// Signal handler for SIGINT (Ctrl+C)
// Must match: fn (c_int) callconv(.c) void
fn handle_sigint(sig: c_int) callconv(.c) void {
    _ = sig;
    g_should_quit = true;
}

pub fn main() !void {
    // 1) Open DB via zigPortfolio
    var handle: DbHandle = 0;
    const db_path = "../db/portfolio.db"; // cwd is frontendConnector/

    const rc_open = zp.zp_sqlite_open(db_path, &handle);
    if (rc_open != zp.ZP_ERROR_OK) {
        std.debug.print("❌ Failed to open DB (err={d})\n", .{rc_open});
        return error.DbOpenFailed;
    }

    // Ensure we close DB on normal exit
    defer {
        const rc_close = zp.zp_sqlite_close(handle);
        if (rc_close != zp.ZP_ERROR_OK) {
            std.debug.print("⚠️ Failed to close DB (err={d})\n", .{rc_close});
        } else {
            std.debug.print("✅ Closed DB\n", .{});
        }
    }

    std.debug.print("✅ Opened DB. Handle = 0x{x}\n", .{handle});

    // 2) Version info sanity check
    const major = zp.zp_version_major();
    const minor = zp.zp_version_minor();
    const patch = zp.zp_version_patch();
    std.debug.print("Version: {d}.{d}.{d}\n", .{ major, minor, patch });

    var ver_buf: [32]u8 = undefined;
    const written = zp.zp_version_string(&ver_buf, ver_buf.len);
    std.debug.print("Version string: {s}\n", .{ver_buf[0..written]});

    // 3) Build Address 127.0.0.1:8080
    const addr = try Address.parseIpAndPort("127.0.0.1:8080");

    // 4) Create and configure listening socket (NON-BLOCKING)

    const nonblock: u32 = if (@hasDecl(posix.SOCK, "NONBLOCK")) posix.SOCK.NONBLOCK else 0;
    const sock_flags = posix.SOCK.STREAM | posix.SOCK.CLOEXEC | nonblock;
    const proto: u32 = if (addr.any.family == posix.AF.UNIX) 0 else posix.IPPROTO.TCP;

    const sockfd = try posix.socket(addr.any.family, sock_flags, proto);
    defer posix.close(sockfd);

    // Allow address/port reuse
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

    // Install Ctrl+C handler
    _ = c.signal(c.SIGINT, handle_sigint);

    std.debug.print("🌐 Listening on http://127.0.0.1:8080 (Ctrl+C to stop gracefully)\n", .{});

    // 5) Accept connections in a loop and send a tiny HTTP response
    while (!g_should_quit) {
        var accepted_addr: Address = undefined;
        var addr_len: posix.socklen_t = @sizeOf(Address);

        const fd = posix.accept(
            sockfd,
            &accepted_addr.any,
            &addr_len,
            posix.SOCK.CLOEXEC,
        ) catch |err| switch (err) {
            // Non-blocking socket: no pending connection → WouldBlock
            error.WouldBlock => {
                // No std.time.sleep in 0.15; just spin and re-check g_should_quit.
                continue;
            },
            else => {
                if (g_should_quit) {
                    std.debug.print("ℹ️ accept() error while shutting down: {any}\n", .{err});
                    break;
                }
                return err;
            },
        };

        if (g_should_quit) {
            posix.close(fd);
            break;
        }

        var stream = Stream{ .handle = fd };
        std.debug.print("🔌 New connection from {any}\n", .{accepted_addr});

        const response =
            "HTTP/1.1 200 OK\r\n" ++ "Content-Type: text/plain\r\n" ++ "Content-Length: 12\r\n" ++ "\r\n" ++ "Hello world\n";

        _ = try stream.write(response);
        stream.close();
    }

    std.debug.print("👋 Ctrl+C detected, shutting down gracefully.\n", .{});
}
