const std = @import("std");
const helper = @import("helper.zig");
const schema = @import("schemaStructs.zig");

const fifoSnapshot = @import("fifoSnapshot.zig");
const readTrades = @import("readTrades.zig");
const meta = @import("sqliteMeta.zig");

const DbHandle = helper.DbHandle;

// Load-only prototype: retains buffers in memory for now.
var g_prev_snapshot: ?[]schema.zp_fifo_snapshot = null;
var g_year_trades: ?[]schema.zp_trade = null;

fn clear_cached_buffers() void {
    const allocator = std.heap.c_allocator;

    if (g_prev_snapshot) |buf| {
        allocator.free(buf);
        g_prev_snapshot = null;
    }
    if (g_year_trades) |buf| {
        allocator.free(buf);
        g_year_trades = null;
    }
}

pub fn sqlite_process_year_load_only(handle: DbHandle, year: u32) helper.ErrorCode {
    if (handle == helper.INVALID_DB_HANDLE) return .invalid_argument;
    if (year == 0) return .invalid_argument;

    clear_cached_buffers();
    const allocator = std.heap.c_allocator;

    // 1) Previous year snapshot
    const prev_year: u32 = year - 1;
    var prev_snapshot_rows: usize = 0;

    var ec = meta.sqlite_count_rows(handle, .fifo_snapshot, prev_year, null, null, &prev_snapshot_rows);
    if (ec != .ok) return ec;

    if (prev_snapshot_rows > 0) {
        var snapshot_buf = allocator.alloc(schema.zp_fifo_snapshot, prev_snapshot_rows) catch return .preparation_fail;
        errdefer allocator.free(snapshot_buf);

        var got: usize = 0;
        ec = fifoSnapshot.sqlite_read_fifo_snapshot_by_tax_year(
            handle,
            prev_year,
            snapshot_buf.ptr,
            snapshot_buf.len,
            &got,
        );
        if (ec != .ok) return ec;

        g_prev_snapshot = snapshot_buf[0..got];
    }

    // 2) Current year trades
    var year_trade_rows: usize = 0;

    ec = meta.sqlite_count_rows(handle, .trades, year, null, null, &year_trade_rows);
    if (ec != .ok) {
        clear_cached_buffers();
        return ec;
    }

    if (year_trade_rows > 0) {
        var trades_buf = allocator.alloc(schema.zp_trade, year_trade_rows) catch {
            clear_cached_buffers();
            return .preparation_fail;
        };
        errdefer allocator.free(trades_buf);

        var got: usize = 0;
        ec = readTrades.sqlite_read_trades_by_year(handle, year, trades_buf.ptr, trades_buf.len, &got);
        if (ec != .ok) {
            clear_cached_buffers();
            return ec;
        }

        g_year_trades = trades_buf[0..got];
    }

    return .ok;
}
