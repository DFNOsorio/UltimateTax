const db = @import("db.zig");
const server = @import("net.zig");

/// Entry point for the frontendConnector executable.
///
/// Responsibilities:
/// - Open the zigPortfolio database
/// - Log library version information
/// - Run the TCP/HTTP server
/// - Ensure resources are released on exit
pub fn main() !void {
    const handle = try db.openDefaultDb();
    defer db.closeDb(handle);

    db.logLibraryVersion();

    try server.runServer(handle);
}
