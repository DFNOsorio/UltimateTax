const std = @import("std");

// Fixed-size, C-compatible buffers for passing trades across the C ABI.
//
// Conventions:
// - Required text fields: must be non-empty C strings.
// - Defaultable text fields: empty C string (first byte == 0) means "use DB default".
// - Defaultable numeric fields: sentinel values mean "use DB default":
//     * commission: NaN or < 0 => default 0.0
//     * conversion_rate_eur: NaN or <= 0 => default 1.0

pub const ZP_TRADE_DATETIME_CAP: usize = 17; // "YYYY-MM-DD HH:MM" + '\0'
pub const ZP_TRADE_TICKER_CAP: usize = 16;
pub const ZP_TRADE_BROKER_CAP: usize = 8;
pub const ZP_TRADE_TYPE_CAP: usize = 8;
pub const ZP_TRADE_COUNTRY_CAP: usize = 8;
pub const ZP_TRADE_CURRENCY_CAP: usize = 8;

pub const zp_trade = extern struct {
    trade_datetime: [ZP_TRADE_DATETIME_CAP]u8,
    ticker: [ZP_TRADE_TICKER_CAP]u8,

    quantity: f64,
    price_per_share: f64,

    broker: [ZP_TRADE_BROKER_CAP]u8,
    trade_type: [ZP_TRADE_TYPE_CAP]u8,

    commission: f64,

    country: [ZP_TRADE_COUNTRY_CAP]u8,
    currency: [ZP_TRADE_CURRENCY_CAP]u8,

    conversion_rate_eur: f64,
};

pub const zp_broker_name = extern struct {
    name: [ZP_TRADE_BROKER_CAP]u8,
};

pub const zp_year = extern struct {
    value: u32,
};

pub fn clearTrade(t: *zp_trade) void {
    @memset(t.trade_datetime[0..], 0);
    @memset(t.ticker[0..], 0);
    @memset(t.broker[0..], 0);
    @memset(t.trade_type[0..], 0);
    @memset(t.country[0..], 0);
    @memset(t.currency[0..], 0);

    t.quantity = 0;
    t.price_per_share = 0;
    t.commission = std.math.nan(f64);
    t.conversion_rate_eur = std.math.nan(f64);
}

pub fn clearBrokerName(b: *zp_broker_name) void {
    @memset(b.name[0..], 0);
}
