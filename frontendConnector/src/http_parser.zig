const std = @import("std");
const mem = std.mem;

pub const Method = enum { Get, Post, Other };

pub const RequestView = struct {
    method: Method,
    path: []const u8,
    body: []const u8,
};

/// Returned when a full request is present in the buffer.
pub const ParseResult = struct {
    req: RequestView,
    /// Number of bytes consumed from the buffer (headers + body).
    consumed: usize,
};

fn headerEndIndex(buf: []const u8) ?usize {
    if (mem.indexOf(u8, buf, "\r\n\r\n")) |idx| return idx + 4;
    return null;
}

fn parseContentLength(headers: []const u8) usize {
    var it = mem.splitSequence(u8, headers, "\r\n");
    while (it.next()) |line_raw| {
        const line = mem.trim(u8, line_raw, " \t\r\n");
        if (line.len == 0) continue;

        if (mem.startsWith(u8, line, "Content-Length:") or mem.startsWith(u8, line, "content-length:")) {
            const colon = mem.indexOfScalar(u8, line, ':') orelse continue;
            const rest = mem.trim(u8, line[colon + 1 ..], " \t");
            return std.fmt.parseUnsigned(usize, rest, 10) catch 0;
        }
    }
    return 0;
}

fn parseRequestLine(headers: []const u8) ?struct { method: Method, path: []const u8 } {
    const line_end = mem.indexOfScalar(u8, headers, '\n') orelse return null;
    const first_line = mem.trim(u8, headers[0..line_end], " \r\n");

    var it = mem.splitScalar(u8, first_line, ' ');
    const method_str = it.next() orelse return null;
    const path = it.next() orelse return null;

    const method: Method =
        if (mem.eql(u8, method_str, "GET")) .Get else if (mem.eql(u8, method_str, "POST")) .Post else .Other;

    return .{ .method = method, .path = path };
}

/// If `buf` contains a complete HTTP request (headers + body), returns:
/// - parsed request slices *into buf*
/// - the consumed byte count (for pipelining)
///
/// Assumes non-chunked requests.
pub fn tryParseFullRequest(buf: []const u8) ?ParseResult {
    const header_end = headerEndIndex(buf) orelse return null;
    const headers = buf[0 .. header_end - 4];

    const line = parseRequestLine(headers) orelse return null;

    const content_len = parseContentLength(headers);
    const total = header_end + content_len;
    if (buf.len < total) return null;

    const body = buf[header_end..total];

    return .{
        .req = .{
            .method = line.method,
            .path = line.path,
            .body = body,
        },
        .consumed = total,
    };
}
