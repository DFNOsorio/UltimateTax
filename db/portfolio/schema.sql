-- ============================================================================
-- UltimateTax portfolio schema (SQLite)
--
-- Documentation style:
-- - Each section starts with purpose + key constraints.
-- - Index comments explain the query pattern they optimize.
-- - ISO datetime columns use text format checks where relevant.
-- ============================================================================

-- ============================================================================
-- trades
-- Purpose: Raw imported trade operations (BUY/SELL).
-- Notes:
-- - `trade_year` is a STORED generated column for year filters and aggregates.
-- - `trade_datetime` uses format `YYYY-MM-DD HH:MM`.
-- ============================================================================

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

-- Indexes for trade timelines and year-based filters.

-- Global timeline scans.
CREATE INDEX IF NOT EXISTS idx_trades_datetime
    ON trades(trade_datetime, id);

-- Per-ticker timeline scans.
CREATE INDEX IF NOT EXISTS idx_trades_ticker_datetime
    ON trades(ticker, trade_datetime, id);

-- Broker + ticker timeline scans.
CREATE INDEX IF NOT EXISTS idx_trades_broker_ticker_datetime
    ON trades(broker, ticker, trade_datetime, id);

-- Year aggregate/filter paths.
CREATE INDEX IF NOT EXISTS idx_trades_year
    ON trades(trade_year);

CREATE INDEX IF NOT EXISTS idx_trades_year_broker
    ON trades(trade_year, broker);

CREATE INDEX IF NOT EXISTS idx_trades_year_broker_ticker
    ON trades(trade_year, broker, ticker);


-- ============================================================================
-- fifo_snapshot
-- Purpose: Open FIFO lots carried at year end / current state.
-- Notes:
-- - One row per open lot with acquisition linkage to `trades`.
-- - Market quote fields store original quote currency price and FX rate to EUR.
-- - `current_lot_value_eur` is a VIRTUAL generated value using quote FX conversion.
-- ============================================================================

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
    last_price_update_date TEXT NOT NULL DEFAULT '',
    last_updated_stock_price REAL NOT NULL DEFAULT 0.0 CHECK (last_updated_stock_price >= 0.0),
    last_updated_stock_currency TEXT NOT NULL DEFAULT 'EUR',
    last_updated_stock_conversion_rate_eur REAL NOT NULL DEFAULT 1.0 CHECK (last_updated_stock_conversion_rate_eur > 0.0),
    current_lot_value_eur REAL GENERATED ALWAYS AS (
        COALESCE(qty_remaining, 0.0) *
        (
            COALESCE(last_updated_stock_price, 0.0) /
            COALESCE(last_updated_stock_conversion_rate_eur, 1.0)
        )
    ) VIRTUAL,

    -- Metadata
    country             TEXT NOT NULL,

    FOREIGN KEY (acq_trade_id) REFERENCES trades(id)
);

-- Main listing path: broker/year/ticker ordered by acquisition time.
CREATE INDEX IF NOT EXISTS idx_fifo_snapshot_broker_year_ticker
ON fifo_snapshot(broker, tax_year, ticker, acq_datetime, lot_id);

-- Fast reverse lookup from acquisition trade to lot.
CREATE INDEX IF NOT EXISTS idx_fifo_snapshot_acq_trade
ON fifo_snapshot(acq_trade_id);


-- ============================================================================
-- fifo_snapshot_action_applied
-- Purpose: Many-to-many link of snapshot lots to corporate actions already applied.
-- Notes:
-- - Composite primary key prevents duplicate application of the same action to a lot.
-- ============================================================================

CREATE TABLE IF NOT EXISTS fifo_snapshot_action_applied (
    lot_id       INTEGER NOT NULL,
    action_id    INTEGER NOT NULL,

    PRIMARY KEY (lot_id, action_id),

    FOREIGN KEY (lot_id) REFERENCES fifo_snapshot(lot_id) ON DELETE CASCADE,
    FOREIGN KEY (action_id) REFERENCES corporate_actions(action_id)
);

-- Action-centric traversal of applied lots.
CREATE INDEX IF NOT EXISTS idx_fifo_snapshot_action_applied_action
ON fifo_snapshot_action_applied(action_id, lot_id);


-- ============================================================================
-- fifo_realized
-- Purpose: Realized FIFO matches between SELL and BUY lots.
-- Notes:
-- - `gain_eur` is a VIRTUAL generated value.
-- - `(sell_trade_id, match_seq)` enforces deterministic match ordering.
-- ============================================================================

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

-- Broker/year totals.
CREATE INDEX IF NOT EXISTS idx_fifo_realized_broker_year
ON fifo_realized(broker, tax_year);

-- Broker/year/ticker listings in sell order.
CREATE INDEX IF NOT EXISTS idx_fifo_realized_broker_year_ticker
ON fifo_realized(broker, tax_year, ticker, sell_datetime, realized_id);

-- Sell-trade reverse lookup.
CREATE INDEX IF NOT EXISTS idx_fifo_realized_sell_trade
ON fifo_realized(sell_trade_id);


-- ============================================================================
-- dividends
-- Purpose: Dividend cashflow rows with derived analytics fields.
-- Notes:
-- - `dividend_year` is STORED for filtering.
-- - `number_of_shares` and `tax_rate` are VIRTUAL computed values.
-- ============================================================================

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

-- Dividend listings by year and broker/year.
CREATE INDEX IF NOT EXISTS idx_div_year
ON dividends(dividend_year, dividend_dt, dividend_id);

CREATE INDEX IF NOT EXISTS idx_div_broker_year
ON dividends(broker, dividend_year, dividend_dt, dividend_id);


-- ============================================================================
-- options_operations
-- Purpose: Options operations used for tax reporting flows.
-- Notes:
-- - `bought_year` is STORED for yearly filtering.
-- - Datetime format checks enforce `YYYY-MM-DD HH:MM`.
-- ============================================================================

CREATE TABLE IF NOT EXISTS options_operations (
    option_id            INTEGER PRIMARY KEY AUTOINCREMENT,

    broker               TEXT NOT NULL,
    ticker               TEXT NOT NULL,
    country              TEXT NOT NULL,

    bought_dt            TEXT NOT NULL,
    expiration_dt        TEXT NOT NULL,

    bought_year          INTEGER
                         GENERATED ALWAYS AS (CAST(substr(bought_dt, 1, 4) AS INTEGER)) STORED,

    amount_x100          INTEGER NOT NULL CHECK (amount_x100 <> 0),
    per_contract         REAL NOT NULL CHECK (per_contract >= 0.0),
    tax                  REAL NOT NULL CHECK (tax >= 0.0),

    currency             TEXT NOT NULL,
    conversion_rate_eur  REAL NOT NULL CHECK (conversion_rate_eur > 0.0),

    CHECK (
        length(bought_dt) = 16
        AND substr(bought_dt, 5, 1) = '-'
        AND substr(bought_dt, 8, 1) = '-'
        AND substr(bought_dt, 11, 1) = ' '
        AND substr(bought_dt, 14, 1) = ':'
    ),
    CHECK (
        length(expiration_dt) = 16
        AND substr(expiration_dt, 5, 1) = '-'
        AND substr(expiration_dt, 8, 1) = '-'
        AND substr(expiration_dt, 11, 1) = ' '
        AND substr(expiration_dt, 14, 1) = ':'
    )
);

-- Timeline + broker/year/ticker query accelerators.
CREATE INDEX IF NOT EXISTS idx_options_bought_dt
ON options_operations(bought_dt, option_id);

CREATE INDEX IF NOT EXISTS idx_options_ticker_bought_dt
ON options_operations(ticker, bought_dt, option_id);

CREATE INDEX IF NOT EXISTS idx_options_broker_ticker_bought_dt
ON options_operations(broker, ticker, bought_dt, option_id);

CREATE INDEX IF NOT EXISTS idx_options_year
ON options_operations(bought_year);

CREATE INDEX IF NOT EXISTS idx_options_year_broker
ON options_operations(bought_year, broker);

CREATE INDEX IF NOT EXISTS idx_options_year_broker_ticker
ON options_operations(bought_year, broker, ticker);


-- ============================================================================
-- corporate_actions
-- Purpose: Corporate action history used to transform lots and symbols.
-- Notes:
-- - `action_year` and `ratio` are STORED generated fields.
-- - SPLIT requires `to_ticker` = NULL; other action types require non-NULL.
-- ============================================================================

CREATE TABLE IF NOT EXISTS corporate_actions (
    action_id     INTEGER PRIMARY KEY AUTOINCREMENT,

    broker        TEXT NOT NULL,

    -- ISO date: "YYYY-MM-DD"
    action_date   TEXT NOT NULL,

    action_year   INTEGER
                 GENERATED ALWAYS AS (CAST(substr(action_date, 1, 4) AS INTEGER)) STORED,

    action_type   TEXT NOT NULL
                 CHECK (action_type IN ('MERGER','CONVERSION','SPINOFF','SPLIT','CASH')),

    from_ticker   TEXT NOT NULL,
    to_ticker     TEXT,  -- NULL when SPLIT

    from_qty      REAL NOT NULL CHECK (from_qty > 0.0),
    to_qty        REAL NOT NULL CHECK (to_qty > 0.0),

    -- ratio = to_qty / from_qty
    ratio         REAL
                 GENERATED ALWAYS AS (to_qty / from_qty) STORED,

    -- Structural checks
    CHECK (
        length(action_date) = 10
        AND substr(action_date, 5, 1) = '-'
        AND substr(action_date, 8, 1) = '-'
    ),

    -- Enforce to_ticker NULL only for SPLIT
    CHECK (
        ((action_type = 'SPLIT' OR action_type = 'CASH') AND to_ticker IS NULL)
        OR
        ((action_type <> 'SPLIT' AND action_type <> 'CASH') AND to_ticker IS NOT NULL)
    )
);

-- Corporate action lookup indexes.
CREATE INDEX IF NOT EXISTS idx_ca_broker_date
ON corporate_actions(broker, action_date, action_id);

CREATE INDEX IF NOT EXISTS idx_ca_broker_year_type
ON corporate_actions(broker, action_year, action_type, action_date, action_id);

CREATE INDEX IF NOT EXISTS idx_ca_from_ticker_date
ON corporate_actions(from_ticker, action_date, action_id);

CREATE INDEX IF NOT EXISTS idx_ca_to_ticker_date
ON corporate_actions(to_ticker, action_date, action_id);
