const std = @import("std");

pub const TRADE_DATETIME_CAP: usize = 17; // "YYYY-MM-DD HH:MM" (16) + '\0'
pub const TICKER_CAP: usize = 16;
pub const BROKER_CAP: usize = 8;
pub const TRADE_TYPE_CAP: usize = 5; // "BUY" / "SELL" + '\0'
pub const COUNTRY_CAP: usize = 3;
pub const CURRENCY_CAP: usize = 4;

/// Fixed-size, C-ABI-safe representation for both inserting and reading.
///
/// String fields are NUL-terminated C strings stored inline.
/// - Required: trade_datetime, ticker must be non-empty (first byte != 0).
/// - Optional/defaultable: broker, trade_type, country, currency -> empty string means "use default".
/// - commission: NaN or <0 means "use default".
/// - conversion_rate_eur: NaN or <=0 means "use default".
pub const zp_trade = extern struct {
    // Required (non-empty C strings)
    trade_datetime: [TRADE_DATETIME_CAP]u8,
    ticker: [TICKER_CAP]u8,

    // Required numerics
    quantity: f64,
    price_per_share: f64,

    // Optional/defaultable (empty string => use default)
    broker: [BROKER_CAP]u8,
    trade_type: [TRADE_TYPE_CAP]u8,

    // Optional/defaultable numeric
    commission: f64,

    // Optional/defaultable
    country: [COUNTRY_CAP]u8,
    currency: [CURRENCY_CAP]u8,

    // Optional/defaultable numeric
    conversion_rate_eur: f64,
};

pub inline fn clearTrade(t: *zp_trade) void {
    t.* = std.mem.zeroes(zp_trade);
}
