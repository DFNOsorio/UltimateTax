comptime {
    _ = @import("insert_trade_tests.zig");
    _ = @import("read_trade_by_id_tests.zig");
    _ = @import("read_trades_by_year_tests.zig");
    _ = @import("read_trades_by_broker_tests.zig");
    _ = @import("read_trades_by_year_and_broker_tests.zig");
    _ = @import("read_all_trades_tests.zig");
    _ = @import("get_unique_meta_tests.zig");
    _ = @import("fifo_snapshot_tests.zig");
    _ = @import("fifo_realized_tests.zig");
    _ = @import("sqlite_count_rows_tests.zig");
    _ = @import("insert_dividend_tests.zig");
    _ = @import("read_dividends_tests.zig");
    _ = @import("read_dividends_year_country_tests.zig");
}
