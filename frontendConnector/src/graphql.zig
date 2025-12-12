const std = @import("std");
const mem = std.mem;

const db = @import("db.zig");

pub const GraphQLError = error{
    OutputTooSmall,
};

/// Execute a GraphQL request.
///
/// `request_body` is the HTTP body (either raw GraphQL or a small JSON wrapper).
/// `out` is a caller-provided output buffer for the JSON response body.
pub fn execute(database: *db.Db, request_body: []const u8, out: []u8) GraphQLError![]const u8 {
    const doc = extractGraphQLDocument(request_body);

    // Ultra-minimal "parser": detect supported fields by substring.
    // This is intentionally simple for learning; we’ll harden later.
    if (mem.indexOf(u8, doc, "addMock") != null) {
        // DB call (mockable)
        database.addMockRecord() catch {};

        return writeJson(out, "{\"data\":{\"addMock\":true}}");
    }

    if (mem.indexOf(u8, doc, "hello") != null) {
        return writeJson(out, "{\"data\":{\"hello\":\"world\"}}");
    }

    return writeJson(out, "{\"errors\":[{\"message\":\"Unsupported query\"}]}");
}

/// If the body looks like JSON with a `"query"` key, extract that string.
/// Otherwise treat the entire body as the GraphQL document.
///
/// Limitations (for now):
/// - JSON query string must be a simple quoted string (no escape handling)
fn extractGraphQLDocument(body: []const u8) []const u8 {
    const trimmed = mem.trim(u8, body, " \t\r\n");
    if (trimmed.len == 0) return trimmed;

    if (trimmed[0] != '{') return trimmed;

    // Very small JSON "query" extractor:
    // {"query":"..."}
    const key = "\"query\"";
    const key_pos = mem.indexOf(u8, trimmed, key) orelse return trimmed;

    // Find ':' after "query"
    const after_key = trimmed[key_pos + key.len ..];
    const colon_pos = mem.indexOfScalar(u8, after_key, ':') orelse return trimmed;

    var p = mem.trimLeft(u8, after_key[colon_pos + 1 ..], " \t\r\n");
    if (p.len == 0 or p[0] != '"') return trimmed;

    p = p[1..]; // skip opening quote
    const end_quote = mem.indexOfScalar(u8, p, '"') orelse return trimmed;

    return p[0..end_quote];
}

fn writeJson(out: []u8, s: []const u8) GraphQLError![]const u8 {
    if (s.len > out.len) return error.OutputTooSmall;
    mem.copyForwards(u8, out[0..s.len], s);
    return out[0..s.len];
}
