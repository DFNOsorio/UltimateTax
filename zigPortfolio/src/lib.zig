pub const sqlite = @import("sqliteConnector.zig");
pub const meta = @import("sqliteMeta.zig");
pub const helper = @import("helper.zig");
pub const schema = @import("schemaStructs.zig");

// Re-export C ABI functions for Zig callers/tests (tests call api.zp_* directly).
pub const zp_sqlite_open = sqlite.zp_sqlite_open;
pub const zp_sqlite_close = sqlite.zp_sqlite_close;

pub const zp_sqlite_insert_trade = sqlite.zp_sqlite_insert_trade;
pub const zp_sqlite_insert_trade_struct = sqlite.zp_sqlite_insert_trade_struct;

pub const zp_sqlite_read_trade_by_id = sqlite.zp_sqlite_read_trade_by_id;
pub const zp_sqlite_read_trades_by_year = sqlite.zp_sqlite_read_trades_by_year;
pub const zp_sqlite_read_trades_by_broker = sqlite.zp_sqlite_read_trades_by_broker;
pub const zp_sqlite_read_trades_by_year_and_broker = sqlite.zp_sqlite_read_trades_by_year_and_broker;
pub const zp_sqlite_read_all_trades = sqlite.zp_sqlite_read_all_trades;

pub const zp_sqlite_get_unique_brokers = sqlite.zp_sqlite_get_unique_brokers;
pub const zp_sqlite_get_unique_years = sqlite.zp_sqlite_get_unique_years;

pub const zp_sqlite_count_rows = sqlite.zp_sqlite_count_rows;
pub const zp_table = sqlite.zp_table;

pub const zp_version_major = sqlite.zp_version_major;
pub const zp_version_minor = sqlite.zp_version_minor;
pub const zp_version_patch = sqlite.zp_version_patch;
pub const zp_version_string = sqlite.zp_version_string;

pub const zp_sqlite_insert_fifo_snapshot = sqlite.zp_sqlite_insert_fifo_snapshot;
pub const zp_sqlite_read_fifo_snapshot_all = sqlite.zp_sqlite_read_fifo_snapshot_all;
pub const zp_sqlite_read_fifo_snapshot_by_tax_year = sqlite.zp_sqlite_read_fifo_snapshot_by_tax_year;
pub const zp_sqlite_read_fifo_snapshot_by_ticker_per_year = sqlite.zp_sqlite_read_fifo_snapshot_by_ticker_per_year;
pub const zp_sqlite_read_fifo_snapshot_by_broker_per_year = sqlite.zp_sqlite_read_fifo_snapshot_by_broker_per_year;

pub const zp_sqlite_insert_fifo_realized = sqlite.zp_sqlite_insert_fifo_realized;
pub const zp_sqlite_read_fifo_realized_all = sqlite.zp_sqlite_read_fifo_realized_all;
pub const zp_sqlite_read_fifo_realized_by_tax_year = sqlite.zp_sqlite_read_fifo_realized_by_tax_year;
pub const zp_sqlite_read_fifo_realized_by_ticker_per_year = sqlite.zp_sqlite_read_fifo_realized_by_ticker_per_year;
pub const zp_sqlite_read_fifo_realized_by_broker_per_year = sqlite.zp_sqlite_read_fifo_realized_by_broker_per_year;
