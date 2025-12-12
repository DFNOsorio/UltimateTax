const std = @import("std");
const mem = std.mem;

const db = @import("db.zig");
const graphql = @import("graphql.zig");
const parser = @import("http_parser.zig");

pub const Response = struct {
    status: []const u8,
    content_type: []const u8,
    body: []const u8,
};

/// Route a parsed request to a response.
/// `scratch` is used for generated bodies (GraphQL JSON).
pub fn route(db_handle: db.DbHandle, req: parser.RequestView, scratch: []u8) Response {
    if (req.method == .Get and mem.eql(u8, req.path, "/")) {
        return .{ .status = "200 OK", .content_type = "text/plain", .body = "Hello world\n" };
    }

    if (req.method == .Post and mem.eql(u8, req.path, "/graphql")) {
        const json_body = graphql.execute(db_handle, req.body, scratch) catch {
            return .{
                .status = "500 Internal Server Error",
                .content_type = "application/json",
                .body = "{\"errors\":[{\"message\":\"Internal error\"}]}",
            };
        };

        return .{
            .status = "200 OK",
            .content_type = "application/json",
            .body = json_body,
        };
    }

    return .{ .status = "404 Not Found", .content_type = "text/plain", .body = "Not Found\n" };
}

/// Build a full HTTP response (headers + body) into `out`.
pub fn buildResponseBytes(resp: Response, out: []u8) ![]const u8 {
    const header = try std.fmt.bufPrint(
        out,
        "HTTP/1.1 {s}\r\nContent-Type: {s}\r\nContent-Length: {d}\r\n\r\n",
        .{ resp.status, resp.content_type, resp.body.len },
    );

    const total_len = header.len + resp.body.len;
    if (total_len > out.len) return error.OutputTooSmall;

    mem.copyForwards(u8, out[header.len..total_len], resp.body);
    return out[0..total_len];
}
