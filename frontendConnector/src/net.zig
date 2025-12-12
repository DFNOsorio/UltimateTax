const std = @import("std");

const posix = std.posix;
const mem = std.mem;

const Address = std.net.Address;

const db = @import("db.zig");
const http = @import("http.zig");
const parser = @import("http_parser.zig");

/// kqueue + kevent types/constants (NO signal.h here)
const c = @cImport({
    @cInclude("sys/types.h");
    @cInclude("sys/event.h");
    @cInclude("sys/time.h");
    @cInclude("fcntl.h");
});

/// macOS SIGINT is 2. (If you later want portability, we can add a platform switch.)
const SIGINT: c_int = 2;

/// C signal handler type and function declaration (no cImport needed).
const SigHandler = *const fn (c_int) callconv(.c) void;
extern "c" fn signal(sig: c_int, handler: SigHandler) SigHandler;

/// No-op handler: required so the signal is “handled/ignored” and delivered to kqueue
fn sig_noop(_: c_int) callconv(.c) void {}

const ConnState = enum { Reading, Writing };

const Conn = struct {
    fd: posix.fd_t,
    state: ConnState = .Reading,

    rbuf: [16 * 1024]u8 = undefined,
    rlen: usize = 0,

    wbuf: [16 * 1024]u8 = undefined,
    wlen: usize = 0,
    wsent: usize = 0,

    scratch: [4096]u8 = undefined,
};

pub fn runServer(db_handle: db.DbHandle) !void {
    const addr = try Address.parseIpAndPort("127.0.0.1:8080");

    const nonblock: u32 = if (@hasDecl(posix.SOCK, "NONBLOCK")) posix.SOCK.NONBLOCK else 0;
    const sock_flags = posix.SOCK.STREAM | posix.SOCK.CLOEXEC | nonblock;
    const proto: u32 = if (addr.any.family == posix.AF.UNIX) 0 else posix.IPPROTO.TCP;

    const listen_fd = try posix.socket(addr.any.family, sock_flags, proto);
    defer posix.close(listen_fd);

    try posix.setsockopt(listen_fd, posix.SOL.SOCKET, posix.SO.REUSEADDR, &mem.toBytes(@as(i32, 1)));
    if (@hasDecl(posix.SO, "REUSEPORT") and addr.any.family != posix.AF.UNIX) {
        try posix.setsockopt(listen_fd, posix.SOL.SOCKET, posix.SO.REUSEPORT, &mem.toBytes(@as(i32, 1)));
    }

    const socklen = addr.getOsSockLen();
    try posix.bind(listen_fd, &addr.any, socklen);
    try posix.listen(listen_fd, 128);

    // Create kqueue
    const kq = c.kqueue();
    if (kq < 0) return error.KqueueFailed;
    defer posix.close(@intCast(kq));

    // Install SIGINT handler so the default “terminate” does not happen.
    _ = signal(SIGINT, sig_noop);

    // Register:
    // - listen_fd readable (EV_CLEAR => drain accept queue)
    // - SIGINT as EVFILT_SIGNAL
    var changes: [2]c.struct_kevent = .{
        makeKevent(@intCast(listen_fd), c.EVFILT_READ, @intCast(c.EV_ADD | c.EV_ENABLE), @intCast(c.EV_CLEAR), 0, null),
        makeKevent(@as(usize, @intCast(SIGINT)), c.EVFILT_SIGNAL, @intCast(c.EV_ADD | c.EV_ENABLE), 0, 0, null),
    };

    if (c.kevent(kq, &changes, @intCast(changes.len), null, 0, null) < 0)
        return error.KeventRegisterFailed;

    std.debug.print("🌐 Listening on http://127.0.0.1:8080 (Ctrl+C to stop)\n", .{});
    std.debug.print("   POST /graphql\n", .{});

    var conns = std.AutoHashMap(posix.fd_t, *Conn).init(std.heap.page_allocator);
    defer {
        var it = conns.iterator();
        while (it.next()) |e| {
            const conn = e.value_ptr.*;
            _ = posix.close(conn.fd);
            std.heap.page_allocator.destroy(conn);
        }
        conns.deinit();
    }

    var events: [64]c.struct_kevent = undefined;
    var should_quit = false;

    while (!should_quit) {
        const nev = c.kevent(kq, null, 0, &events, @intCast(events.len), null);
        if (nev < 0) return error.KeventWaitFailed;

        for (events[0..@intCast(nev)]) |ev| {
            // SIGINT -> shutdown
            if (ev.filter == c.EVFILT_SIGNAL and ev.ident == @as(usize, @intCast(SIGINT))) {
                should_quit = true;
                break;
            }

            // Listener readable -> accept all pending
            if (ev.filter == c.EVFILT_READ and ev.ident == @as(usize, @intCast(listen_fd))) {
                try acceptAll(kq, listen_fd, &conns);
                continue;
            }

            // Client events
            const fd: posix.fd_t = @intCast(ev.ident);
            const conn_ptr = conns.get(fd) orelse continue;
            const conn = conn_ptr;

            if ((ev.flags & c.EV_EOF) != 0) {
                closeConn(kq, &conns, conn);
                continue;
            }

            if (ev.filter == c.EVFILT_READ) {
                _ = handleRead(kq, db_handle, &conns, conn);
            } else if (ev.filter == c.EVFILT_WRITE) {
                handleWrite(kq, &conns, conn);
            }
        }
    }

    std.debug.print("👋 Ctrl+C detected, shutting down gracefully.\n", .{});
}

fn acceptAll(kq: c_int, listen_fd: posix.fd_t, conns: *std.AutoHashMap(posix.fd_t, *Conn)) !void {
    while (true) {
        var client_addr: Address = undefined;
        var addr_len: posix.socklen_t = @sizeOf(Address);

        const fd = posix.accept(listen_fd, &client_addr.any, &addr_len, posix.SOCK.CLOEXEC) catch |err| switch (err) {
            error.WouldBlock => break,
            else => return err,
        };

        setNonBlocking(fd);

        const conn = try std.heap.page_allocator.create(Conn);
        conn.* = .{ .fd = fd };
        try conns.put(fd, conn);

        std.debug.print("🔌 New connection from ", .{});
        printAddress(client_addr);
        std.debug.print("\n", .{});

        var kev = makeKevent(@intCast(fd), c.EVFILT_READ, @intCast(c.EV_ADD | c.EV_ENABLE), @intCast(c.EV_CLEAR), 0, null);
        if (c.kevent(kq, &kev, 1, null, 0, null) < 0) return error.KeventRegisterFailed;
    }
}

fn handleRead(kq: c_int, db_handle: db.DbHandle, conns: *std.AutoHashMap(posix.fd_t, *Conn), conn: *Conn) bool {
    while (conn.rlen < conn.rbuf.len) {
        const got = posix.read(conn.fd, conn.rbuf[conn.rlen..]) catch |err| switch (err) {
            error.WouldBlock => break,
            else => {
                closeConn(kq, conns, conn);
                return false;
            },
        };

        if (got == 0) {
            closeConn(kq, conns, conn);
            return false;
        }

        conn.rlen += got;

        if (parser.tryParseFullRequest(conn.rbuf[0..conn.rlen])) |req| {
            const resp = http.route(db_handle, req, conn.scratch[0..]);

            const out_bytes = http.buildResponseBytes(resp, conn.wbuf[0..]) catch blk: {
                const fallback = http.buildResponseBytes(
                    .{ .status = "500 Internal Server Error", .content_type = "text/plain", .body = "Response too large\n" },
                    conn.wbuf[0..],
                ) catch {
                    closeConn(kq, conns, conn);
                    return false;
                };
                break :blk fallback;
            };

            conn.wlen = out_bytes.len;
            conn.wsent = 0;
            conn.state = .Writing;

            var kev = makeKevent(@intCast(conn.fd), c.EVFILT_WRITE, @intCast(c.EV_ADD | c.EV_ENABLE), @intCast(c.EV_CLEAR), 0, null);
            _ = c.kevent(kq, &kev, 1, null, 0, null);

            break;
        }
    }
    return true;
}

fn handleWrite(kq: c_int, conns: *std.AutoHashMap(posix.fd_t, *Conn), conn: *Conn) void {
    while (conn.wsent < conn.wlen) {
        const wrote = posix.write(conn.fd, conn.wbuf[conn.wsent..conn.wlen]) catch |err| switch (err) {
            error.WouldBlock => return,
            else => {
                closeConn(kq, conns, conn);
                return;
            },
        };
        conn.wsent += wrote;
    }
    closeConn(kq, conns, conn);
}

fn closeConn(kq: c_int, conns: *std.AutoHashMap(posix.fd_t, *Conn), conn: *Conn) void {
    var del_read = makeKevent(@intCast(conn.fd), c.EVFILT_READ, @intCast(c.EV_DELETE), 0, 0, null);
    var del_write = makeKevent(@intCast(conn.fd), c.EVFILT_WRITE, @intCast(c.EV_DELETE), 0, 0, null);
    _ = c.kevent(kq, &del_read, 1, null, 0, null);
    _ = c.kevent(kq, &del_write, 1, null, 0, null);

    _ = posix.close(conn.fd);
    _ = conns.remove(conn.fd);
    std.heap.page_allocator.destroy(conn);
}

fn setNonBlocking(fd: posix.fd_t) void {
    const cur = c.fcntl(fd, c.F_GETFL, @as(c_int, 0));
    if (cur < 0) return;
    _ = c.fcntl(fd, c.F_SETFL, @as(c_int, cur | c.O_NONBLOCK));
}

fn makeKevent(ident: usize, filter: i16, flags: u16, fflags: u32, data: i64, udata: ?*anyopaque) c.struct_kevent {
    return .{ .ident = ident, .filter = filter, .flags = flags, .fflags = fflags, .data = data, .udata = udata };
}

fn printAddress(addr: Address) void {
    switch (addr.any.family) {
        posix.AF.INET => {
            const a = std.mem.bigToNative(u32, addr.in.sa.addr);
            const b0: u8 = @truncate(a >> 24);
            const b1: u8 = @truncate(a >> 16);
            const b2: u8 = @truncate(a >> 8);
            const b3: u8 = @truncate(a);

            const port = std.mem.bigToNative(u16, addr.in.sa.port);
            std.debug.print("{d}.{d}.{d}.{d}:{d}", .{ b0, b1, b2, b3, port });
        },
        else => std.debug.print("[family={d}]", .{addr.any.family}),
    }
}
