pub const zp_trade = extern struct {
    trade_datetime: ?[*:0]const u8,
    ticker: ?[*:0]const u8,
    quantity: f64,
    price_per_share: f64,
    broker: ?[*:0]const u8,
    type: ?[*:0]const u8,
    commission: f64,
    country: ?[*:0]const u8,
    currency: ?[*:0]const u8,
    conversion_rate_eur: f64,
};
