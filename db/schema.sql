-- db/schema.sql
-- Simple schema for Revolut stock trades

PRAGMA foreign_keys = ON;

-- Drop existing table if we are recreating the DB
DROP TABLE IF EXISTS trades;

CREATE TABLE trades (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,

    broker              TEXT NOT NULL,

    -- Local datetime of the trade in ISO format: "YYYY-MM-DD HH:MM"
    trade_datetime      TEXT NOT NULL,

    -- BUY or SELL
    type                TEXT NOT NULL CHECK (type IN ('BUY', 'SELL')),

    -- Stock ticker symbol, e.g. "OSTK", "GE"
    ticker              TEXT NOT NULL,

    -- Number of shares
    quantity            INTEGER NOT NULL,

    -- Price per share in trade currency
    price_per_share     REAL NOT NULL,

    -- Commission in trade currency
    commission          REAL NOT NULL DEFAULT 0.0,

    -- Country of the market (e.g. "US")
    country             TEXT,

    -- Trade currency (e.g. "USD")
    currency            TEXT NOT NULL,

    -- How many units of this currency correspond to 1 EUR
    conversion_rate_eur REAL NOT NULL
);

-- Helpful indexes

-- Order everything by time quickly
CREATE INDEX idx_trades_datetime
    ON trades(trade_datetime);

-- Per-ticker queries in time order
CREATE INDEX idx_trades_ticker_datetime
    ON trades(ticker, trade_datetime);
