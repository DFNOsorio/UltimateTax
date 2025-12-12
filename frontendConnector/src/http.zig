const std = @import("std");
const mem = std.mem;

const db = @import("db.zig");

/// Supported HTTP methods for our tiny server.
pub const HttpMethod = enum {
    Get,
    Post,
    Other,
};

/// Minimal HTTP request representation used by the frontendConnector.
///
/// We only care about the method and path for routing.
pub const HttpRequest = struct {
    method: HttpMethod,
    path: []const u8,
};

/// Parse a request line of the form:
///   "METHOD /path HTTP/1.1"
/// into an HttpRequest. Unknown methods are mapped to .Other.
///
/// `line` should not contain the trailing "\r\n".
pub fn parseRequestLine(line: []const u8) HttpRequest {
    var it = mem.splitScalar(u8, line, ' ');

    const method_str = it.next() orelse "";
    const path = it.next() orelse "/";

    const method: HttpMethod = if (mem.eql(u8, method_str, "GET"))
        .Get
    else if (mem.eql(u8, method_str, "POST"))
        .Post
    else
        .Other;

    return .{
        .method = method,
        .path = path,
    };
}

/// Handle a single HTTP connection:
/// - `req_bytes` is the raw bytes read from the socket (at least the first line).
/// - `db_handle` is the portfolio database handle.
/// - `stream` must support `write([]const u8) !usize`.
///
/// For now this implements:
/// - GET /       → "Hello world"
/// - POST /add   → calls db.mockAddRecord and returns a simple text response
pub fn handleConnection(stream: *std.net.Stream, db_handle: db.DbHandle, req_bytes: []const u8) !void {
    // Extract first line: "METHOD PATH HTTP/..."
    const line_end = mem.indexOfScalar(u8, req_bytes, '\n') orelse req_bytes.len;
    const raw_line = req_bytes[0..line_end];
    const first_line = mem.trim(u8, raw_line, " \r\n");

    const req = parseRequestLine(first_line);

    // Route based on method + path
    if (req.method == .Post and mem.eql(u8, req.path, "/add")) {
        try db.mockAddRecord(db_handle);
        try writePlainText(stream, "Add endpoint called (mock)\n");
        return;
    }

    // Default response for everything else
    try writePlainText(stream, "Hello world\n");
}

/// Helper to write a simple 200 OK text/plain HTTP response with a dynamic body.
///
/// `body` must not contain embedded nulls; otherwise it will still be sent,
/// but Content-Length counts bytes, not lines.
fn writePlainText(stream: anytype, body: []const u8) !void {
    var header_buf: [128]u8 = undefined;

    const header = try std.fmt.bufPrint(
        header_buf[0..],
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: {d}\r\n\r\n",
        .{body.len},
    );

    _ = try stream.write(header);
    _ = try stream.write(body);
}
