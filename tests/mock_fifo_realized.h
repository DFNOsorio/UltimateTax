#pragma once

#include "utax_schema.h"
#include <stddef.h>

typedef struct utax_fifo_realized_mock {
    utax_fifo_realized_row row;
    char sell_trade_datetime_key[UTAX_DT_MAX];
    char buy_trade_datetime_key[UTAX_DT_MAX];
} utax_fifo_realized_mock;

const utax_fifo_realized_mock UTAX_MOCK_FIFO_REALIZED[] = {
    {
        .row = {
            .realized_id = 0,
            .broker = "IKBR",
            .tax_year = 2024,
            .ticker = "AAPL",
            .country = "US",
            .sell_trade_id = 0,
            .buy_trade_id = 0,
            .match_seq = 1,
            .sell_datetime = "2024-06-20 15:45",
            .buy_datetime  = "2024-01-10 09:30",
            .qty_matched = 2.0,
            .acquisition_value_eur = 180.0,
            .sale_value_eur = 220.0,
            .costs_eur = 1.0
        },
        .sell_trade_datetime_key = "2024-06-20 15:45",
        .buy_trade_datetime_key  = "2024-01-10 09:30"
    },
    {
        .row = {
            .realized_id = 0,
            .broker = "REVOLUT",
            .tax_year = 2025,
            .ticker = "AAPL",
            .country = "US",
            .sell_trade_id = 0,
            .buy_trade_id = 0,
            .match_seq = 1,
            .sell_datetime = "2026-12-31 16:00",
            .buy_datetime  = "2025-02-05 10:00",
            .qty_matched = 1.0,
            .acquisition_value_eur = 50.0,
            .sale_value_eur = 60.0,
            .costs_eur = 0.5
        },
        .sell_trade_datetime_key = "2026-12-31 16:00",
        .buy_trade_datetime_key  = "2025-02-05 10:00"
    },
    {
        .row = {
            .realized_id = 0,
            .broker = "IKBR",
            .tax_year = 2026,
            .ticker = "MSFT",
            .country = "US",
            .sell_trade_id = 0,
            .buy_trade_id = 0,
            .match_seq = 2,
            .sell_datetime = "2026-12-31 16:00",
            .buy_datetime  = "2026-03-03 09:00",
            .qty_matched = 1.0,
            .acquisition_value_eur = 280.0,
            .sale_value_eur = 500.0,
            .costs_eur = 2.0
        },
        .sell_trade_datetime_key = "2026-12-31 16:00",
        .buy_trade_datetime_key  = "2026-03-03 09:00"
    }
};

inline size_t utax_mock_fifo_realized_count(void) {
    return sizeof(UTAX_MOCK_FIFO_REALIZED) / sizeof(UTAX_MOCK_FIFO_REALIZED[0]);
}
