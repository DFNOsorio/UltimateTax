#pragma once

#include "utax_schema.h"
#include <stddef.h>

typedef struct utax_fifo_snapshot_action_applied_mock {
    utax_fifo_snapshot_action_applied_row row;
    char acq_trade_datetime_key[UTAX_DT_MAX];
    char action_date_key[11];
    char action_type_key[UTAX_ACTION_TYPE_MAX];
    char from_ticker_key[UTAX_TICKER_MAX];
} utax_fifo_snapshot_action_applied_mock;

typedef struct utax_fifo_snapshot_corporate_action_mock {
    utax_corporate_actions_row row;
} utax_fifo_snapshot_corporate_action_mock;

static const utax_fifo_snapshot_action_applied_mock UTAX_MOCK_FIFO_SNAPSHOT_ACTION_APPLIED[] = {
    {
        .row = { .lot_id = 0, .action_id = 0 },
        .acq_trade_datetime_key = "2024-01-10 09:30",
        .action_date_key = "2025-07-30",
        .action_type_key = "SPLIT",
        .from_ticker_key = "AAPL"
    },
    {
        .row = { .lot_id = 0, .action_id = 0 },
        .acq_trade_datetime_key = "2026-03-03 09:00",
        .action_date_key = "2026-08-01",
        .action_type_key = "CONVERSION",
        .from_ticker_key = "MSFT"
    }
};

static const utax_fifo_snapshot_corporate_action_mock UTAX_MOCK_FIFO_SNAPSHOT_CORPORATE_ACTIONS[] = {
    {
        .row = {
            .action_id = 0,
            .from_qty = 2.0,
            .to_qty = 1.0,
            .ratio = 0.0,
            .action_year = 0,
            ._pad0 = 0,
            .broker = "IKBR",
            .action_date = "2025-07-30",
            .action_type = "SPLIT",
            .from_ticker = "AAPL",
            .to_ticker = ""
        }
    },
    {
        .row = {
            .action_id = 0,
            .from_qty = 1.0,
            .to_qty = 1.0,
            .ratio = 0.0,
            .action_year = 0,
            ._pad0 = 0,
            .broker = "IKBR",
            .action_date = "2026-08-01",
            .action_type = "CONVERSION",
            .from_ticker = "MSFT",
            .to_ticker = "MSF2"
        }
    },
    {
        .row = {
            .action_id = 0,
            .from_qty = 1.0,
            .to_qty = 1.0,
            .ratio = 0.0,
            .action_year = 0,
            ._pad0 = 0,
            .broker = "IKBR",
            .action_date = "2026-09-01",
            .action_type = "CONVERSION",
            .from_ticker = "MSFT",
            .to_ticker = "MSF3"
        }
    },
    {
        .row = {
            .action_id = 0,
            .from_qty = 1.0,
            .to_qty = 1.0,
            .ratio = 0.0,
            .action_year = 0,
            ._pad0 = 0,
            .broker = "REVO -> IKBR",
            .action_date = "2023-05-10",
            .action_type = "CONVERSION",
            .from_ticker = "WFC",
            .to_ticker = "WFC"
        }
    },
    {
        .row = {
            .action_id = 0,
            .from_qty = 1.0,
            .to_qty = 1.0,
            .ratio = 0.0,
            .action_year = 0,
            ._pad0 = 0,
            .broker = "REVO -> IKBR",
            .action_date = "2023-05-10",
            .action_type = "CONVERSION",
            .from_ticker = "SPCE",
            .to_ticker = "SPCE"
        }
    }
};

static inline size_t utax_mock_fifo_snapshot_action_applied_count(void) {
    return sizeof(UTAX_MOCK_FIFO_SNAPSHOT_ACTION_APPLIED) / sizeof(UTAX_MOCK_FIFO_SNAPSHOT_ACTION_APPLIED[0]);
}

static inline size_t utax_mock_fifo_snapshot_corporate_actions_count(void) {
    return sizeof(UTAX_MOCK_FIFO_SNAPSHOT_CORPORATE_ACTIONS) / sizeof(UTAX_MOCK_FIFO_SNAPSHOT_CORPORATE_ACTIONS[0]);
}
