const std = @import("std");

const db_mock = @import("db_mock.zig");
const http = @import("http.zig");
const parser = @import("http_parser.zig");
const graphql = @import("graphql.zig");

test "http_parser parses a full GET request" {
    const raw =
        "GET / HTTP/1.1\r\n" ++
        "Host: localhost\r\n" ++
        "\r\n";

    const pr = parser.tryParseFullRequest(raw) orelse return error.TestExpectedNonNull;
    try std.testing.expectEqual(parser.Method.Get, pr.req.method);
    try std.testing.expect(std.mem.eql(u8, pr.req.path, "/"));
    try std.testing.expectEqual(@as(usize, raw.len), pr.consumed);
    try std.testing.expectEqual(@as(usize, 0), pr.req.body.len);
}

test "http_parser parses a POST with Content-Length" {
    const body = "{\"query\":\"hello\"}";
    var buf: [256]u8 = undefined;
    const raw = try std.fmt.bufPrint(
        &buf,
        "POST /graphql HTTP/1.1\r\nHost: localhost\r\nContent-Length: {d}\r\n\r\n{s}",
        .{ body.len, body },
    );

    const pr = parser.tryParseFullRequest(raw) orelse return error.TestExpectedNonNull;
    try std.testing.expectEqual(parser.Method.Post, pr.req.method);
    try std.testing.expect(std.mem.eql(u8, pr.req.path, "/graphql"));
    try std.testing.expect(std.mem.eql(u8, pr.req.body, body));
    try std.testing.expectEqual(@as(usize, raw.len), pr.consumed);
}

test "graphql.execute returns hello" {
    var ctx = db_mock.Context{};
    var db = db_mock.asDb(&ctx);

    var out: [128]u8 = undefined;
    const json = try graphql.execute(&db, "hello", &out);

    try std.testing.expect(std.mem.eql(u8, json, "{\"data\":{\"hello\":\"world\"}}"));
    try std.testing.expectEqual(@as(usize, 0), ctx.add_mock_calls);
}

test "graphql.execute addMock calls the DB port" {
    var ctx = db_mock.Context{};
    var db = db_mock.asDb(&ctx);

    var out: [128]u8 = undefined;
    const json = try graphql.execute(&db, "mutation { addMock }", &out);

    try std.testing.expect(std.mem.eql(u8, json, "{\"data\":{\"addMock\":true}}"));
    try std.testing.expectEqual(@as(usize, 1), ctx.add_mock_calls);
}

test "http.route / returns hello world" {
    var ctx = db_mock.Context{};
    var db = db_mock.asDb(&ctx);

    const req: parser.RequestView = .{
        .method = .Get,
        .path = "/",
        .body = "",
    };
    var scratch: [64]u8 = undefined;
    const resp = http.route(&db, req, &scratch);

    try std.testing.expect(std.mem.eql(u8, resp.status, "200 OK"));
    try std.testing.expect(std.mem.eql(u8, resp.content_type, "text/plain"));
    try std.testing.expect(std.mem.eql(u8, resp.body, "Hello world\n"));
}

test "http.route /graphql uses graphql.execute" {
    var ctx = db_mock.Context{};
    var db = db_mock.asDb(&ctx);

    const req: parser.RequestView = .{
        .method = .Post,
        .path = "/graphql",
        .body = "{\"query\":\"hello\"}",
    };
    var scratch: [256]u8 = undefined;
    const resp = http.route(&db, req, &scratch);

    try std.testing.expect(std.mem.eql(u8, resp.status, "200 OK"));
    try std.testing.expect(std.mem.eql(u8, resp.content_type, "application/json"));
    try std.testing.expect(std.mem.eql(u8, resp.body, "{\"data\":{\"hello\":\"world\"}}"));
}
