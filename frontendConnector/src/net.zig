const std = @import("std");

const posix = std.posix;
const mem = std.mem;

const Address = std.net.Address;

const db = @import("db.zig");
const http = @import("http.zig");
const parser = @import("http_parser.zig");
const graphql = @import("graphql.zig");

/// kqueue + fcntl (NO signal.h; Zig sometimes fails translating SIG_IGN on macOS)
const c = @cImport({
    @cInclude("sys/types.h");
    @cInclude("sys/event.h");
    @cInclude("sys/time.h");
    @cInclude("fcntl.h");
});

/// macOS SIGINT == 2
const SIGINT: c_int = 2;

/// Declare C `signal()` manually (avoid @cImport(signal.h)).
const SigHandler = *const fn (c_int) callconv(.c) void;
extern "c" fn signal(sig: c_int, handler: SigHandler) SigHandler;

/// No-op handler. We want SIGINT to become a kqueue event, not terminate the process.
fn sig_noop(_: c_int) callconv(.c) void {}

const ConnState = enum { Reading, Pending, Writing };

const Conn = struct {
    fd: posix.fd_t,
    state: ConnState = .Reading,

    // read accumulation buffer
    rbuf: [16 * 1024]u8 = undefined,
    rlen: usize = 0,

    // write buffer (full HTTP response bytes)
    wbuf: [16 * 1024]u8 = undefined,
    wlen: usize = 0,
    wsent: usize = 0,

    // per-conn scratch for non-GraphQL routing (optional; route() may use it)
    scratch: [4096]u8 = undefined,
};

/// Worker thread: jobs and completions.
/// We keep everything in this file to ensure Zig 0.15 compatibility and avoid
/// depending on ArrayList.init() APIs that changed.
const Worker = struct {
    const Job = struct {
        fd: posix.fd_t,
        body: []u8, // owned copy of GraphQL body
    };

    const Completion = struct {
        fd: posix.fd_t,
        response: []u8, // owned full HTTP response bytes
    };

    alloc: std.mem.Allocator,
    db_handle: db.DbHandle,
    wake_fd: posix.fd_t,

    mu: std.Thread.Mutex = .{},
    cv: std.Thread.Condition = .{},
    stop: bool = false,

    jobs: std.ArrayListUnmanaged(Job) = .{},
    done: std.ArrayListUnmanaged(Completion) = .{},

    fn deinit(self: *Worker) void {
        // best-effort cleanup
        for (self.jobs.items) |j| self.alloc.free(j.body);
        for (self.done.items) |d| self.alloc.free(d.response);
        self.jobs.deinit(self.alloc);
        self.done.deinit(self.alloc);
    }

    fn requestStop(self: *Worker) void {
        self.mu.lock();
        self.stop = true;
        self.cv.signal();
        self.mu.unlock();
    }

    fn submit(self: *Worker, fd: posix.fd_t, body: []const u8) !void {
        const owned = try self.alloc.alloc(u8, body.len);
        mem.copyForwards(u8, owned, body);

        self.mu.lock();
        defer self.mu.unlock();

        try self.jobs.append(self.alloc, .{ .fd = fd, .body = owned });
        self.cv.signal();
    }

    /// Drain completions into `out` (caller-owned list).
    fn drain(self: *Worker, out: *std.ArrayListUnmanaged(Completion)) !void {
        self.mu.lock();
        defer self.mu.unlock();

        // move items
        for (self.done.items) |item| {
            try out.append(self.alloc, item);
        }
        self.done.clearRetainingCapacity();
    }

    fn threadMain(self: *Worker) void {
        while (true) {
            self.mu.lock();
            while (self.jobs.items.len == 0 and !self.stop) {
                self.cv.wait(&self.mu);
            }
            if (self.stop) {
                self.mu.unlock();
                return;
            }

            // pop last job
            const idx = self.jobs.items.len - 1;
            const job = self.jobs.items[idx];
            self.jobs.items.len -= 1;

            self.mu.unlock();

            // Execute GraphQL (JSON body)
            var json_scratch: [4096]u8 = undefined;
            const json_body = graphql.execute(self.db_handle, job.body, &json_scratch) catch
                "{\"errors\":[{\"message\":\"Internal error\"}]}";

            const resp: http.Response = .{
                .status = "200 OK",
                .content_type = "application/json",
                .body = json_body,
            };

            // Build full HTTP response into tmp, then allocate owned copy
            var tmp: [16 * 1024]u8 = undefined;
            const bytes = http.buildResponseBytes(resp, &tmp) catch blk: {
                const fallback: http.Response = .{
                    .status = "500 Internal Server Error",
                    .content_type = "text/plain",
                    .body = "Response too large\n",
                };
                break :blk http.buildResponseBytes(fallback, &tmp) catch tmp[0..0];
            };

            const owned_resp = self.alloc.alloc(u8, bytes.len) catch {
                self.alloc.free(job.body);
                continue;
            };
            mem.copyForwards(u8, owned_resp, bytes);

            // Free job body
            self.alloc.free(job.body);

            // Push completion
            self.mu.lock();
            const ok = self.done.append(self.alloc, .{ .fd = job.fd, .response = owned_resp }) catch {
                self.mu.unlock();
                self.alloc.free(owned_resp);
                continue;
            };
            _ = ok;
            self.mu.unlock();

            // Wake main thread via pipe (best effort)
            const one: [1]u8 = .{1};
            _ = posix.write(self.wake_fd, &one) catch {};
        }
    }
};

pub fn runServer(db_handle: db.DbHandle) !void {
    const alloc = std.heap.c_allocator;

    // Build address
    const addr = try Address.parseIpAndPort("127.0.0.1:8080");

    // Listening socket: non-blocking so acceptAll can drain until WouldBlock.
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

    // kqueue
    const kq = c.kqueue();
    if (kq < 0) return error.KqueueFailed;
    defer posix.close(@intCast(kq));

    // Route SIGINT to kqueue event rather than default terminate.
    _ = signal(SIGINT, sig_noop);

    // Worker wake pipe
    const pipe_fds = try posix.pipe();
    const wake_read = pipe_fds[0];
    const wake_write = pipe_fds[1];
    defer posix.close(wake_read);
    defer posix.close(wake_write);
    setNonBlocking(wake_read);

    // Worker runtime + thread
    var worker: Worker = .{
        .alloc = alloc,
        .db_handle = db_handle,
        .wake_fd = wake_write,
    };
    defer worker.deinit();

    var worker_thread = try std.Thread.spawn(.{}, Worker.threadMain, .{&worker});
    defer {
        worker.requestStop();
        worker_thread.join();
    }

    // Register events:
    // - listener read
    // - SIGINT
    // - wake pipe read
    var changes: [3]c.struct_kevent = .{
        makeKevent(@intCast(listen_fd), c.EVFILT_READ, @intCast(c.EV_ADD | c.EV_ENABLE), @intCast(c.EV_CLEAR), 0, null),
        makeKevent(@as(usize, @intCast(SIGINT)), c.EVFILT_SIGNAL, @intCast(c.EV_ADD | c.EV_ENABLE), 0, 0, null),
        makeKevent(@intCast(wake_read), c.EVFILT_READ, @intCast(c.EV_ADD | c.EV_ENABLE), @intCast(c.EV_CLEAR), 0, null),
    };

    if (c.kevent(kq, &changes, @intCast(changes.len), null, 0, null) < 0)
        return error.KeventRegisterFailed;

    std.debug.print("🌐 Listening on http://127.0.0.1:8080 (Ctrl+C to stop)\n", .{});
    std.debug.print("   GET  /\n", .{});
    std.debug.print("   POST /graphql (handled by worker thread)\n", .{});

    // Connection table
    var conns = std.AutoHashMap(posix.fd_t, *Conn).init(alloc);
    defer {
        var it = conns.iterator();
        while (it.next()) |e| {
            const conn = e.value_ptr.*;
            _ = posix.close(conn.fd);
            alloc.destroy(conn);
        }
        conns.deinit();
    }

    var events: [64]c.struct_kevent = undefined;
    var should_quit = false;

    while (!should_quit) {
        const nev = c.kevent(kq, null, 0, &events, @intCast(events.len), null);
        if (nev < 0) return error.KeventWaitFailed;

        for (events[0..@intCast(nev)]) |ev| {
            // SIGINT -> graceful stop
            if (ev.filter == c.EVFILT_SIGNAL and ev.ident == @as(usize, @intCast(SIGINT))) {
                should_quit = true;
                break;
            }

            // Wake pipe -> drain completions and arm writes
            if (ev.filter == c.EVFILT_READ and ev.ident == @as(usize, @intCast(wake_read))) {
                drainWakePipe(wake_read);
                try applyWorkerCompletions(kq, alloc, &worker, &conns);
                continue;
            }

            // Listener ready -> accept many
            if (ev.filter == c.EVFILT_READ and ev.ident == @as(usize, @intCast(listen_fd))) {
                try acceptAll(kq, alloc, listen_fd, &conns);
                continue;
            }

            // Client sockets
            const fd: posix.fd_t = @intCast(ev.ident);
            const conn = conns.get(fd) orelse continue; // conn: *Conn

            if ((ev.flags & c.EV_EOF) != 0) {
                closeConn(kq, alloc, &conns, conn);
                continue;
            }

            if (ev.filter == c.EVFILT_READ) {
                const ok = try handleRead(kq, alloc, db_handle, &worker, &conns, conn);
                if (!ok) continue;
            } else if (ev.filter == c.EVFILT_WRITE) {
                handleWrite(kq, alloc, db_handle, &worker, &conns, conn);
            }
        }
    }

    std.debug.print("👋 Ctrl+C detected, shutting down gracefully.\n", .{});
}

fn acceptAll(kq: c_int, alloc: std.mem.Allocator, listen_fd: posix.fd_t, conns: *std.AutoHashMap(posix.fd_t, *Conn)) !void {
    while (true) {
        var client_addr: Address = undefined;
        var addr_len: posix.socklen_t = @sizeOf(Address);

        const fd = posix.accept(listen_fd, &client_addr.any, &addr_len, posix.SOCK.CLOEXEC) catch |err| switch (err) {
            error.WouldBlock => break,
            else => return err,
        };

        setNonBlocking(fd);

        const conn = try alloc.create(Conn);
        conn.* = .{ .fd = fd };

        try conns.put(fd, conn);

        std.debug.print("🔌 New connection from ", .{});
        printAddress(client_addr);
        std.debug.print("\n", .{});

        // Register READ events for this client
        var kev = makeKevent(@intCast(fd), c.EVFILT_READ, @intCast(c.EV_ADD | c.EV_ENABLE), @intCast(c.EV_CLEAR), 0, null);
        if (c.kevent(kq, &kev, 1, null, 0, null) < 0) return error.KeventRegisterFailed;
    }
}

/// Keep-alive model:
/// - We process ONE request at a time per connection.
/// - While pending (GraphQL offloaded) or writing, READ is disabled.
/// - After response sent, we re-enable READ and reset request buffer.
///
/// No pipelining yet (client must not send next request before we re-enable read).
fn handleRead(
    kq: c_int,
    alloc: std.mem.Allocator,
    db_handle: db.DbHandle,
    worker: *Worker,
    conns: *std.AutoHashMap(posix.fd_t, *Conn),
    conn: *Conn,
) !bool {
    if (conn.state != .Reading) return true;

    while (conn.rlen < conn.rbuf.len) {
        const got = posix.read(conn.fd, conn.rbuf[conn.rlen..]) catch |err| switch (err) {
            error.WouldBlock => break,
            else => {
                closeConn(kq, alloc, conns, conn);
                return false;
            },
        };

        if (got == 0) {
            closeConn(kq, alloc, conns, conn);
            return false;
        }

        conn.rlen += got;

        // Too large / not parsed -> close for now
        if (conn.rlen == conn.rbuf.len and parser.tryParseFullRequest(conn.rbuf[0..conn.rlen]) == null) {
            closeConn(kq, alloc, conns, conn);
            return false;
        }

        if (parser.tryParseFullRequest(conn.rbuf[0..conn.rlen])) |pr| {
            // Consume only what we used; keep any pipelined bytes.
            consumeFromReadBuffer(conn, pr.consumed);

            const req = pr.req;

            if (req.method == .Post and mem.eql(u8, req.path, "/graphql")) {
                try worker.submit(conn.fd, req.body);

                conn.state = .Pending;
                disableFilter(kq, conn.fd, c.EVFILT_READ);
                return true;
            }

            const resp = http.route(db_handle, req, conn.scratch[0..]);
            const out_bytes = http.buildResponseBytes(resp, conn.wbuf[0..]) catch {
                closeConn(kq, alloc, conns, conn);
                return false;
            };

            conn.wlen = out_bytes.len;
            conn.wsent = 0;
            conn.state = .Writing;

            disableFilter(kq, conn.fd, c.EVFILT_READ);
            enableWrite(kq, conn.fd);
            return true;
        }
    }

    return true;
}

fn handleWrite(
    kq: c_int,
    alloc: std.mem.Allocator,
    db_handle: db.DbHandle,
    worker: *Worker,
    conns: *std.AutoHashMap(posix.fd_t, *Conn),
    conn: *Conn,
) void {
    if (conn.state != .Writing) return;

    while (conn.wsent < conn.wlen) {
        const wrote = posix.write(conn.fd, conn.wbuf[conn.wsent..conn.wlen]) catch |err| switch (err) {
            error.WouldBlock => return,
            else => {
                closeConn(kq, alloc, conns, conn);
                return;
            },
        };
        conn.wsent += wrote;
    }

    // Done writing response. Keep-alive: go back to Reading.
    conn.wlen = 0;
    conn.wsent = 0;
    conn.state = .Reading;

    // Disable WRITE interest; re-enable READ.
    disableFilter(kq, conn.fd, c.EVFILT_WRITE);
    enableFilter(kq, conn.fd, c.EVFILT_READ);

    if (conn.rlen > 0) {
        _ = handleRead(kq, alloc, db_handle, worker, conns, conn) catch {
            closeConn(kq, alloc, conns, conn);
        };
    }
}

fn applyWorkerCompletions(
    kq: c_int,
    alloc: std.mem.Allocator,
    worker: *Worker,
    conns: *std.AutoHashMap(posix.fd_t, *Conn),
) !void {
    var list: std.ArrayListUnmanaged(Worker.Completion) = .{};
    defer list.deinit(worker.alloc);

    try worker.drain(&list);

    for (list.items) |comp| {
        defer worker.alloc.free(comp.response);

        const conn = conns.get(comp.fd) orelse continue;

        // If connection is no longer pending/valid, drop.
        if (conn.state != .Pending) continue;

        if (comp.response.len > conn.wbuf.len) {
            closeConn(kq, alloc, conns, conn);
            continue;
        }

        mem.copyForwards(u8, conn.wbuf[0..comp.response.len], comp.response);
        conn.wlen = comp.response.len;
        conn.wsent = 0;
        conn.state = .Writing;

        // Enable write, keep read disabled until write completes (then keep-alive back to read).
        enableWrite(kq, conn.fd);
    }
}

fn closeConn(kq: c_int, alloc: std.mem.Allocator, conns: *std.AutoHashMap(posix.fd_t, *Conn), conn: *Conn) void {
    // Remove filters best-effort
    deleteFilter(kq, conn.fd, c.EVFILT_READ);
    deleteFilter(kq, conn.fd, c.EVFILT_WRITE);

    _ = posix.close(conn.fd);
    _ = conns.remove(conn.fd);
    alloc.destroy(conn);
}

fn makeKevent(ident: usize, filter: i16, flags: u16, fflags: u32, data: i64, udata: ?*anyopaque) c.struct_kevent {
    return .{ .ident = ident, .filter = filter, .flags = flags, .fflags = fflags, .data = data, .udata = udata };
}

fn enableWrite(kq: c_int, fd: posix.fd_t) void {
    // Add/enable write readiness with EV_CLEAR so we drain the socket.
    var kev = makeKevent(@intCast(fd), c.EVFILT_WRITE, @intCast(c.EV_ADD | c.EV_ENABLE), @intCast(c.EV_CLEAR), 0, null);
    _ = c.kevent(kq, &kev, 1, null, 0, null);
}

fn enableFilter(kq: c_int, fd: posix.fd_t, filter: i16) void {
    var kev = makeKevent(@intCast(fd), filter, @intCast(c.EV_ENABLE), 0, 0, null);
    _ = c.kevent(kq, &kev, 1, null, 0, null);
}

fn disableFilter(kq: c_int, fd: posix.fd_t, filter: i16) void {
    var kev = makeKevent(@intCast(fd), filter, @intCast(c.EV_DISABLE), 0, 0, null);
    _ = c.kevent(kq, &kev, 1, null, 0, null);
}

fn deleteFilter(kq: c_int, fd: posix.fd_t, filter: i16) void {
    var kev = makeKevent(@intCast(fd), filter, @intCast(c.EV_DELETE), 0, 0, null);
    _ = c.kevent(kq, &kev, 1, null, 0, null);
}

fn drainWakePipe(wake_read: posix.fd_t) void {
    var buf: [64]u8 = undefined;
    while (true) {
        const n = posix.read(wake_read, &buf) catch |err| switch (err) {
            error.WouldBlock => break,
            else => break,
        };
        if (n == 0) break;
    }
}

fn setNonBlocking(fd: posix.fd_t) void {
    const cur = c.fcntl(@intCast(fd), c.F_GETFL, @as(c_int, 0));
    if (cur < 0) return;
    _ = c.fcntl(@intCast(fd), c.F_SETFL, @as(c_int, cur | c.O_NONBLOCK));
}

fn printAddress(addr: Address) void {
    switch (addr.any.family) {
        posix.AF.INET => {
            const a = mem.bigToNative(u32, addr.in.sa.addr);
            const b0: u8 = @truncate(a >> 24);
            const b1: u8 = @truncate(a >> 16);
            const b2: u8 = @truncate(a >> 8);
            const b3: u8 = @truncate(a);

            const port = mem.bigToNative(u16, addr.in.sa.port);
            std.debug.print("{d}.{d}.{d}.{d}:{d}", .{ b0, b1, b2, b3, port });
        },
        else => std.debug.print("[family={d}]", .{addr.any.family}),
    }
}

fn consumeFromReadBuffer(conn: *Conn, consumed: usize) void {
    if (consumed == 0) return;
    if (consumed >= conn.rlen) {
        conn.rlen = 0;
        return;
    }

    const remaining = conn.rlen - consumed;
    mem.copyForwards(u8, conn.rbuf[0..remaining], conn.rbuf[consumed..conn.rlen]);
    conn.rlen = remaining;
}
