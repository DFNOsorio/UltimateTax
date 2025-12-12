const dbport = @import("db.zig");

/// Minimal mock DB implementation for unit tests.
///
/// Usage:
///   var ctx = db_mock.Context{};
///   var db = db_mock.asDb(&ctx);
///   ... use &db ...
pub const Context = struct {
    add_mock_calls: usize = 0,
};

fn vDeinit(_: *anyopaque) void {
    // No resources.
}

fn vAddMockRecord(ctx_ptr: *anyopaque) anyerror!void {
    const ctx: *Context = @ptrCast(@alignCast(ctx_ptr));
    ctx.add_mock_calls += 1;
}

pub fn asDb(ctx: *Context) dbport.Db {
    return .{
        .ctx = ctx,
        .vtable = &.{
            .deinit = vDeinit,
            .addMockRecord = vAddMockRecord,
        },
    };
}
