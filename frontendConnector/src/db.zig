const std = @import("std");

/// FrontendConnector DB "port".
///
/// `frontendConnector` code should depend only on this interface, not on the
/// zigPortfolio C ABI. This enables deterministic unit tests that do not
/// require a real database file and do not link zigPortfolio.
pub const Db = struct {
    ctx: *anyopaque,
    vtable: *const VTable,

    pub const VTable = struct {
        /// Release any resources owned by `ctx`.
        deinit: *const fn (ctx: *anyopaque) void,

        /// Temporary API used by the current GraphQL handler.
        ///
        /// In production this may execute a real INSERT. In unit tests it is
        /// typically implemented by incrementing a counter or recording calls.
        addMockRecord: *const fn (ctx: *anyopaque) anyerror!void,
    };

    pub fn deinit(self: *Db) void {
        self.vtable.deinit(self.ctx);
    }

    pub fn addMockRecord(self: *Db) !void {
        try self.vtable.addMockRecord(self.ctx);
    }
};
