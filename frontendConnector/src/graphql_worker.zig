const std = @import("std");
const posix = std.posix;

const db = @import("db.zig");
const http = @import("http.zig");
const parser = @import("http_parser.zig");
const graphql = @import("graphql.zig");

/// A GraphQL job for the worker.
pub const Job = struct {
    fd: posix.fd_t,
    /// Owned copy of request body (because the connection read buffer will be reused).
    body: []u8,
};

/// A completed response produced by the worker.
pub const Completion = struct {
    fd: posix.fd_t,
    /// Owned response bytes (HTTP headers + body).
    response: []u8,
};

/// Thread-safe worker runtime: queues + wake pipe.
pub const Runtime = struct {
    alloc: std.mem.Allocator,
    database: *db.Db,

    mu: std.Thread.Mutex = .{},
    cv: std.Thread.Condition = .{},

    jobs: std.ArrayList(Job),
    done: std.ArrayList(Completion),

    stop: bool = false,

    /// write-end of a pipe. Worker writes a byte to wake the main kqueue loop.
    wake_fd: posix.fd_t,

    pub fn init(alloc: std.mem.Allocator, database: *db.Db, wake_fd: posix.fd_t) Runtime {
        return .{
            .alloc = alloc,
            .db_handle = database,
            .jobs = std.ArrayList(Job).init(alloc),
            .done = std.ArrayList(Completion).init(alloc),
            .wake_fd = wake_fd,
        };
    }

    pub fn deinit(self: *Runtime) void {
        // Free any remaining jobs/completions
        for (self.jobs.items) |j| self.alloc.free(j.body);
        for (self.done.items) |c| self.alloc.free(c.response);
        self.jobs.deinit();
        self.done.deinit();
    }

    /// Enqueue a GraphQL job. Called by the main thread.
    pub fn submit(self: *Runtime, fd: posix.fd_t, body: []const u8) !void {
        // Copy body because conn.rbuf is owned by main thread and will be reused.
        const owned = try self.alloc.alloc(u8, body.len);
        std.mem.copyForwards(u8, owned, body);

        self.mu.lock();
        defer self.mu.unlock();

        try self.jobs.append(.{ .fd = fd, .body = owned });
        self.cv.signal();
    }

    /// Drain completions into `out` (caller-owned list). Called by main thread.
    pub fn drainCompletions(self: *Runtime, out: *std.ArrayList(Completion)) !void {
        self.mu.lock();
        defer self.mu.unlock();

        try out.appendSlice(self.done.items);
        self.done.clearRetainingCapacity();
    }

    /// Request worker stop and wake it.
    pub fn requestStop(self: *Runtime) void {
        self.mu.lock();
        self.stop = true;
        self.cv.signal();
        self.mu.unlock();
    }
};

/// Worker thread entry point.
pub fn threadMain(rt: *Runtime) void {
    while (true) {
        rt.mu.lock();
        while (rt.jobs.items.len == 0 and !rt.stop) {
            rt.cv.wait(&rt.mu);
        }
        if (rt.stop) {
            rt.mu.unlock();
            return;
        }

        // Pop one job
        const job = rt.jobs.pop();
        rt.mu.unlock();

        // Execute GraphQL and build full HTTP response
        // - graphql.execute() writes JSON into a scratch buffer; we then wrap into HTTP.
        var json_scratch: [4096]u8 = undefined;
        const json_body = graphql.execute(rt.db_handle, job.body, &json_scratch) catch
            "{\"errors\":[{\"message\":\"Internal error\"}]}";

        const resp: http.Response = .{
            .status = "200 OK",
            .content_type = "application/json",
            .body = json_body,
        };

        // Build HTTP bytes. Allocate an owned buffer for completion.
        // Keep it generous but bounded for now.
        var tmp: [16 * 1024]u8 = undefined;
        const bytes = http.buildResponseBytes(resp, &tmp) catch blk: {
            const fallback: http.Response = .{
                .status = "500 Internal Server Error",
                .content_type = "text/plain",
                .body = "Response too large\n",
            };
            break :blk http.buildResponseBytes(fallback, &tmp) catch tmp[0..0];
        };

        const owned_resp = rt.alloc.alloc(u8, bytes.len) catch {
            // If allocation fails, drop response; still free body.
            rt.alloc.free(job.body);
            continue;
        };
        std.mem.copyForwards(u8, owned_resp, bytes);

        // Free job body now (we no longer need it)
        rt.alloc.free(job.body);

        // Push completion + wake main loop
        rt.mu.lock();
        _ = rt.done.append(.{ .fd = job.fd, .response = owned_resp }) catch {
            rt.mu.unlock();
            rt.alloc.free(owned_resp);
            continue;
        };
        rt.mu.unlock();

        // Wake via pipe (best effort).
        const one: [1]u8 = .{1};
        _ = posix.write(rt.wake_fd, &one) catch {};
    }
}
