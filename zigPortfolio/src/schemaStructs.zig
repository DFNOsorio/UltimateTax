const std = @import("std");

// Fixed ABI sizes (must match zigPortfolio.h)
pub const ZP_BROKER_LEN: usize = 64;
pub const ZP_TICKER_LEN: usize = 32;
pub const ZP_DATETIME_LEN: usize = 32; // "YYYY-MM-DD HH:MM" + '\0'
pub const ZP_TYPE_LEN: usize = 8; // "BUY"/"SELL"
pub const ZP_COUNTRY_LEN: usize = 16; // "US"
pub const ZP_CURRENCY_LEN: usize = 8; // "USD"

pub const zp_broker_buf = [ZP_BROKER_LEN]u8;
pub const zp_ticker = [ZP_TICKER_LEN]u8;
pub const zp_datetime = [ZP_DATETIME_LEN]u8;
pub const zp_trade_type = [ZP_TYPE_LEN]u8;
pub const zp_country = [ZP_COUNTRY_LEN]u8;
pub const zp_currency = [ZP_CURRENCY_LEN]u8;

pub const zp_year = u32;

// IMPORTANT: tests and sqliteMeta expect `.name`
pub const zp_broker_name = extern struct {
    name: zp_broker_buf,

    pub fn zero() zp_broker_name {
        return std.mem.zeroes(zp_broker_name);
    }
};

pub const zp_trade = extern struct {
    id: u32,

    broker: zp_broker_buf,
    trade_datetime: zp_datetime,
    trade_type: zp_trade_type,
    ticker: zp_ticker,

    quantity: f64,
    price_per_share: f64,
    commission: f64,

    country: zp_country,
    currency: zp_currency,
    conversion_rate_eur: f64,

    pub fn zero() zp_trade {
        var t = std.mem.zeroes(zp_trade);
        // Make “unset” numeric fields map to DB defaults in your insert code
        t.commission = std.math.nan(f64);
        t.conversion_rate_eur = std.math.nan(f64);
        return t;
    }
};

pub const zp_fifo_snapshot = extern struct {
    lot_id: u32,

    broker: zp_broker_buf,
    tax_year: zp_year,
    ticker: zp_ticker,

    acq_trade_id: u32,
    acq_datetime: zp_datetime,

    qty_remaining: f64,
    cost_per_share_eur: f64,
    acq_commission_eur: f64,

    country: zp_country,

    pub fn zero() zp_fifo_snapshot {
        var r = std.mem.zeroes(zp_fifo_snapshot);
        r.acq_commission_eur = std.math.nan(f64); // allow DB default 0.0 if your binder uses NULL-on-NaN
        return r;
    }
};

pub const zp_fifo_realized = extern struct {
    realized_id: u32,

    broker: zp_broker_buf,
    tax_year: zp_year,
    ticker: zp_ticker,
    country: zp_country, // IMPORTANT: must be here to match zigPortfolio.h

    sell_trade_id: u32,
    buy_trade_id: u32,
    match_seq: u32,

    sell_datetime: zp_datetime,
    buy_datetime: zp_datetime,

    qty_matched: f64,

    acquisition_value_eur: f64,
    sale_value_eur: f64,
    costs_eur: f64,
    gain_eur: f64,

    pub fn zero() zp_fifo_realized {
        return std.mem.zeroes(zp_fifo_realized);
    }
};

comptime {
    if (@sizeOf(zp_fifo_realized) != 240) {
        @compileError("zp_fifo_realized ABI size mismatch; expected 240 bytes to match zigPortfolio.h");
    }
}

pub const zp_dividend = extern struct {
    dividend_id: u32,

    broker: zp_broker_buf,
    dividend_dt: zp_datetime,
    ticker: zp_ticker,
    country: zp_country,

    per_share: f64,
    total_amount: f64,
    tax: f64,

    currency: zp_currency,
    conversion_rate_eur: f64,

    pub fn zero() zp_dividend {
        var d = std.mem.zeroes(zp_dividend);

        // No DB defaults in your schema for these fields; keep NaN so missing values fail fast.
        d.per_share = std.math.nan(f64);
        d.total_amount = std.math.nan(f64);
        d.tax = std.math.nan(f64);
        d.conversion_rate_eur = std.math.nan(f64);

        return d;
    }
};

// ------------------------------------------------------------------
// Test helpers expected by your existing test suite
// ------------------------------------------------------------------

pub fn clearTrade(t: *zp_trade) void {
    t.* = zp_trade.zero();
}

pub fn clearBrokerName(b: *zp_broker_name) void {
    b.* = zp_broker_name.zero();
}

pub fn clearFifoSnapshot(r: *zp_fifo_snapshot) void {
    r.* = zp_fifo_snapshot.zero();
}

pub fn clearFifoRealized(r: *zp_fifo_realized) void {
    r.* = zp_fifo_realized.zero();
}

pub fn clearDividend(d: *zp_dividend) void {
    d.* = zp_dividend.zero();
}
