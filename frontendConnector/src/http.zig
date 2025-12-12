const std = @import("std");
const mem = std.mem;

const db = @import("db.zig");
const graphql = @import("graphql.zig");

pub const Method = enum { Get, Post, Other };

pub const HttpRequest = struct {
    method: Method,
    path: []const u8,
    body: []const u8,
};

/// Handle one connection: read request, parse, route, respond.
pub fn serveConnection(stream: *std.net.Stream, db_handle: db.DbHandle) !void {
    var buf: [16 * 1024]u8 = undefined;
    const n = try readFullRequest(stream, &buf);
    const bytes = buf[0..n];

    const req = parseRequest(bytes) orelse {
        try writeResponse(stream, "400 Bad Request", "text/plain", "Bad Request\n");
        return;
    };

    // Routing
    if (req.method == .Get and mem.eql(u8, req.path, "/")) {
        try writeResponse(stream, "200 OK", "text/plain", "Hello world\n");
        return;
    }

    if (req.method == .Post and mem.eql(u8, req.path, "/graphql")) {
        var out: [2048]u8 = undefined;
        const json_body = graphql.execute(db_handle, req.body, &out) catch {
            try writeResponse(stream, "500 Internal Server Error", "application/json", "{\"errors\":[{\"message\":\"Internal error\"}]}");
            return;
        };

        try writeResponse(stream, "200 OK", "application/json", json_body);
        return;
    }

    try writeResponse(stream, "404 Not Found", "text/plain", "Not Found\n");
}

/// Read enough bytes to include:
/// - headers (ending at \r\n\r\n)
/// - plus the body as defined by Content-Length (if present)
fn readFullRequest(stream: *std.net.Stream, buf: []u8) !usize {
    var total: usize = 0;

    var header_end_opt: ?usize = null;
    var expected_total: ?usize = null;

    while (total < buf.len) {
        const got = stream.read(buf[total..]) catch |err| switch (err) {
            // Because the accepted socket is non-blocking, read can return WouldBlock
            // until the client request bytes arrive.
            error.WouldBlock => continue,
            else => return err,
        };

        if (got == 0) break; // peer closed
        total += got;

        // Find end of headers if not found yet
        if (header_end_opt == null) {
            if (std.mem.indexOf(u8, buf[0..total], "\r\n\r\n")) |idx| {
                const header_end = idx + 4;
                header_end_opt = header_end;

                const content_len = parseContentLength(buf[0..idx]);
                expected_total = header_end + content_len;

                // If no body, we can stop immediately once headers are complete.
                if (content_len == 0 and total >= header_end) break;
            }
        }

        if (expected_total) |need| {
            if (total >= need) break;
        }
    }

    return total;
}

fn parseRequest(bytes: []const u8) ?HttpRequest {
    const header_end = mem.indexOf(u8, bytes, "\r\n\r\n") orelse return null;
    const headers = bytes[0..header_end];
    const body = bytes[header_end + 4 ..];

    // First line: METHOD PATH HTTP/...
    const line_end = mem.indexOfScalar(u8, headers, '\n') orelse return null;
    const first_line = mem.trim(u8, headers[0..line_end], " \r\n");

    var it = mem.splitScalar(u8, first_line, ' ');
    const method_str = it.next() orelse return null;
    const path = it.next() orelse return null;

    const method: Method = if (mem.eql(u8, method_str, "GET"))
        .Get
    else if (mem.eql(u8, method_str, "POST"))
        .Post
    else
        .Other;

    // Body length should match Content-Length, but we trust readFullRequest for now.
    return .{ .method = method, .path = path, .body = body };
}

fn parseContentLength(headers: []const u8) usize {
    var it = mem.splitSequence(u8, headers, "\r\n");
    while (it.next()) |line_raw| {
        const line = mem.trim(u8, line_raw, " \t\r\n");
        if (line.len == 0) continue;

        // Accept common forms
        if (mem.startsWith(u8, line, "Content-Length:") or mem.startsWith(u8, line, "content-length:")) {
            const colon = mem.indexOfScalar(u8, line, ':') orelse continue;
            const rest = mem.trim(u8, line[colon + 1 ..], " \t");
            return std.fmt.parseUnsigned(usize, rest, 10) catch 0;
        }
    }
    return 0;
}

fn writeResponse(stream: *std.net.Stream, status: []const u8, content_type: []const u8, body: []const u8) !void {
    var header_buf: [256]u8 = undefined;

    const header = try std.fmt.bufPrint(
        header_buf[0..],
        "HTTP/1.1 {s}\r\nContent-Type: {s}\r\nContent-Length: {d}\r\n\r\n",
        .{ status, content_type, body.len },
    );

    _ = try stream.write(header);
    _ = try stream.write(body);
}
