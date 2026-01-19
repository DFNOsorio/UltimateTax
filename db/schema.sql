-- db/schema.sql
-- Simple schema for Revolut stock trades

PRAGMA foreign_keys = ON;

-- Drop existing table if we are recreating the DB
DROP TABLE IF EXISTS trades;

CREATE TABLE trades (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,

    broker              TEXT NOT NULL DEFAULT 'IKBR',

    -- Local datetime of the trade in ISO format: "YYYY-MM-DD HH:MM"
    trade_datetime      TEXT NOT NULL,

    -- BUY or SELL
    type                TEXT NOT NULL CHECK (type IN ('BUY', 'SELL')) DEFAULT 'BUY',

    -- Stock ticker symbol, e.g. "OSTK", "GE"
    ticker              TEXT NOT NULL,

    -- Number of shares
    quantity            REAL NOT NULL,

    -- Price per share in trade currency
    price_per_share     REAL NOT NULL,

    -- Commission in trade currency
    commission          REAL NOT NULL DEFAULT 0.0,

    -- Country of the market (e.g. "US")
    country             TEXT NOT NULL DEFAULT 'US',

    -- Trade currency (e.g. "USD")
    currency            TEXT NOT NULL DEFAULT 'USD',

    -- How many units of this currency correspond to 1 EUR
    conversion_rate_eur REAL NOT NULL DEFAULT 1.0
);

-- Helpful indexes

-- Order everything by time quickly
CREATE INDEX idx_trades_datetime
    ON trades(trade_datetime);

-- Per-ticker queries in time order
CREATE INDEX idx_trades_ticker_datetime
    ON trades(ticker, trade_datetime);

CREATE INDEX IF NOT EXISTS idx_trades_broker_ticker_datetime
    ON trades(broker, ticker, trade_datetime, id);

-- Drop existing table if we are recreating the DB
DROP TABLE IF EXISTS fifo_snapshot;

CREATE TABLE fifo_snapshot (
    lot_id              INTEGER PRIMARY KEY AUTOINCREMENT,

    broker              TEXT NOT NULL,
    tax_year            INTEGER NOT NULL,          -- snapshot as-of end of this year
    ticker              TEXT NOT NULL,

    acq_trade_id        INTEGER NOT NULL,
    acq_datetime        TEXT NOT NULL,

    qty_remaining       REAL NOT NULL CHECK (qty_remaining >= 0.0),

    cost_per_share_eur  REAL NOT NULL CHECK (cost_per_share_eur >= 0.0),

    acq_commission_eur  REAL NOT NULL DEFAULT 0.0 CHECK (acq_commission_eur >= 0.0),

    country             TEXT NOT NULL,

    FOREIGN KEY (acq_trade_id) REFERENCES trades(id)
);

CREATE INDEX IF NOT EXISTS idx_fifo_snapshot_broker_year_ticker
ON fifo_snapshot(broker, tax_year, ticker, acq_datetime, lot_id);

CREATE INDEX IF NOT EXISTS idx_fifo_snapshot_acq_trade
ON fifo_snapshot(acq_trade_id);



DROP TABLE IF EXISTS fifo_realized;

CREATE TABLE IF NOT EXISTS fifo_realized (
  realized_id        INTEGER PRIMARY KEY AUTOINCREMENT,

  broker             TEXT NOT NULL,
  tax_year           INTEGER NOT NULL,
  ticker             TEXT NOT NULL,
  country            TEXT NOT NULL,

  sell_trade_id      INTEGER NOT NULL,
  buy_trade_id       INTEGER NOT NULL,
  match_seq          INTEGER NOT NULL,

  sell_datetime      TEXT NOT NULL,
  buy_datetime       TEXT NOT NULL,

  qty_matched        REAL NOT NULL,

  acquisition_value_eur  REAL NOT NULL,
  sale_value_eur         REAL NOT NULL,
  costs_eur              REAL NOT NULL DEFAULT 0.0,

  gain_eur           REAL GENERATED ALWAYS AS (
                      COALESCE(sale_value_eur, 0.0)
                    - COALESCE(acquisition_value_eur, 0.0)
                    - COALESCE(costs_eur, 0.0)
                  ) VIRTUAL,

  FOREIGN KEY (sell_trade_id) REFERENCES trades(id),
  FOREIGN KEY (buy_trade_id)  REFERENCES trades(id),

  UNIQUE (sell_trade_id, match_seq)
);


CREATE INDEX IF NOT EXISTS idx_fifo_realized_broker_year
ON fifo_realized(broker, tax_year);

CREATE INDEX IF NOT EXISTS idx_fifo_realized_broker_year_ticker
ON fifo_realized(broker, tax_year, ticker, sell_datetime, realized_id);

CREATE INDEX IF NOT EXISTS idx_fifo_realized_sell_trade
ON fifo_realized(sell_trade_id);


DROP TABLE IF EXISTS dividends;

CREATE TABLE IF NOT EXISTS dividends (
    dividend_id         INTEGER PRIMARY KEY AUTOINCREMENT,

    broker              TEXT NOT NULL,
    dividend_dt         TEXT NOT NULL,
    ticker              TEXT NOT NULL,
    country             TEXT NOT NULL,

    per_share           REAL NOT NULL,
    total_amount        REAL NOT NULL,
    tax                 REAL NOT NULL,

    number_of_shares    REAL GENERATED ALWAYS AS (
                        COALESCE(total_amount, 0.0) /
                        COALESCE(per_share, 1.0)
                        ) VIRTUAL,

    tax_rate            REAL GENERATED ALWAYS AS (
                        100.0 *
                        COALESCE(tax, 0.0) /
                        COALESCE(total_amount, 1.0)
                        ) VIRTUAL,

    currency            TEXT NOT NULL,
    conversion_rate_eur REAL NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_div_broker_ticker_dt
ON dividends (broker, ticker, dividend_dt);

CREATE INDEX IF NOT EXISTS idx_div_country_ticker_dt
ON dividends (country, ticker, dividend_dt);
