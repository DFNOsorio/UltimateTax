-- db/schema.sql
-- ─────────────────────────────────────────────────────────────────────────────
-- trades
-- ─────────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS trades (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,

    -- High-selectivity / common filters
    broker              TEXT NOT NULL DEFAULT 'IKBR',
    ticker              TEXT NOT NULL,

    -- Sort key (ISO local datetime)
    trade_datetime      TEXT NOT NULL,

    -- Derived year for fast grouping/filtering
    trade_year          INTEGER
                        GENERATED ALWAYS AS (CAST(substr(trade_datetime, 1, 4) AS INTEGER)) STORED,

    -- BUY or SELL
    type                TEXT NOT NULL CHECK (type IN ('BUY', 'SELL')) DEFAULT 'BUY',

    -- Core numeric payload
    quantity            REAL NOT NULL CHECK (quantity > 0.0),
    price_per_share     REAL NOT NULL CHECK (price_per_share >= 0.0),
    commission          REAL NOT NULL DEFAULT 0.0 CHECK (commission >= 0.0),

    -- Market metadata
    country             TEXT NOT NULL DEFAULT 'US',
    currency            TEXT NOT NULL DEFAULT 'USD',
    conversion_rate_eur REAL NOT NULL DEFAULT 1.0 CHECK (conversion_rate_eur > 0.0),

    -- Structural format check: "YYYY-MM-DD HH:MM"
    CHECK (
        length(trade_datetime) = 16
        AND substr(trade_datetime, 5, 1) = '-'
        AND substr(trade_datetime, 8, 1) = '-'
        AND substr(trade_datetime, 11, 1) = ' '
        AND substr(trade_datetime, 14, 1) = ':'
    )
);

-- Helpful indexes

-- Order everything by time quickly
CREATE INDEX IF NOT EXISTS idx_trades_datetime
    ON trades(trade_datetime, id);

-- Per-ticker queries in time order
CREATE INDEX IF NOT EXISTS idx_trades_ticker_datetime
    ON trades(ticker, trade_datetime, id);

-- Per-broker + ticker queries in time order
CREATE INDEX IF NOT EXISTS idx_trades_broker_ticker_datetime
    ON trades(broker, ticker, trade_datetime, id);

-- Fast year aggregates / filters
CREATE INDEX IF NOT EXISTS idx_trades_year
    ON trades(trade_year);

CREATE INDEX IF NOT EXISTS idx_trades_year_broker
    ON trades(trade_year, broker);

CREATE INDEX IF NOT EXISTS idx_trades_year_broker_ticker
    ON trades(trade_year, broker, ticker);


-- ─────────────────────────────────────────────────────────────────────────────
-- fifo_snapshot
-- ─────────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS fifo_snapshot (
    lot_id              INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Common filters
    broker              TEXT NOT NULL,
    tax_year            INTEGER NOT NULL,
    ticker              TEXT NOT NULL,

    -- Acquisition linkage
    acq_trade_id        INTEGER NOT NULL,
    acq_datetime        TEXT NOT NULL,

    -- Core numeric payload
    qty_remaining       REAL NOT NULL CHECK (qty_remaining >= 0.0),
    cost_per_share_eur  REAL NOT NULL CHECK (cost_per_share_eur >= 0.0),
    acq_commission_eur  REAL NOT NULL DEFAULT 0.0 CHECK (acq_commission_eur >= 0.0),

    -- Metadata
    country             TEXT NOT NULL,

    FOREIGN KEY (acq_trade_id) REFERENCES trades(id)
);

CREATE INDEX IF NOT EXISTS idx_fifo_snapshot_broker_year_ticker
ON fifo_snapshot(broker, tax_year, ticker, acq_datetime, lot_id);

CREATE INDEX IF NOT EXISTS idx_fifo_snapshot_acq_trade
ON fifo_snapshot(acq_trade_id);


-- ─────────────────────────────────────────────────────────────────────────────
-- fifo_realized
-- ─────────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS fifo_realized (
    realized_id        INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Common filters
    broker             TEXT NOT NULL,
    tax_year           INTEGER NOT NULL,
    ticker             TEXT NOT NULL,
    country            TEXT NOT NULL,

    -- Matching identifiers
    sell_trade_id      INTEGER NOT NULL,
    buy_trade_id       INTEGER NOT NULL,
    match_seq          INTEGER NOT NULL,

    -- Timestamps (used for sorting/reporting)
    sell_datetime      TEXT NOT NULL,
    buy_datetime       TEXT NOT NULL,

    -- Core numeric payload
    qty_matched            REAL NOT NULL CHECK (qty_matched > 0.0),
    acquisition_value_eur  REAL NOT NULL CHECK (acquisition_value_eur >= 0.0),
    sale_value_eur         REAL NOT NULL CHECK (sale_value_eur >= 0.0),
    costs_eur              REAL NOT NULL DEFAULT 0.0 CHECK (costs_eur >= 0.0),

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


-- ─────────────────────────────────────────────────────────────────────────────
-- dividends
-- ─────────────────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS dividends (
    dividend_id         INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Common filters
    broker              TEXT NOT NULL,
    ticker              TEXT NOT NULL,
    country             TEXT NOT NULL,

    -- Timestamp
    dividend_dt         TEXT NOT NULL,
    dividend_year       INTEGER
                        GENERATED ALWAYS AS (CAST(substr(dividend_dt, 1, 4) AS INTEGER)) STORED,

    -- Core numeric payload
    per_share           REAL NOT NULL CHECK (per_share > 0.0),
    total_amount        REAL NOT NULL CHECK (total_amount >= 0.0),
    tax                 REAL NOT NULL CHECK (tax >= 0.0),

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
    conversion_rate_eur REAL NOT NULL CHECK (conversion_rate_eur > 0.0)
);

CREATE INDEX IF NOT EXISTS idx_div_year
ON dividends(dividend_year, dividend_dt, dividend_id);

CREATE INDEX IF NOT EXISTS idx_div_broker_year
ON dividends(broker, dividend_year, dividend_dt, dividend_id);
