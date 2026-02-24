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

static inline size_t utax_mock_fifo_snapshot_action_applied_count(void) {
    return sizeof(UTAX_MOCK_FIFO_SNAPSHOT_ACTION_APPLIED) / sizeof(UTAX_MOCK_FIFO_SNAPSHOT_ACTION_APPLIED[0]);
}
