#pragma once

#include "utax_schema.h"
#include <stddef.h>

typedef struct utax_process_year_snapshot_mock {
    utax_fifo_snapshot_row row;
    char acq_trade_datetime_key[UTAX_DT_MAX];
} utax_process_year_snapshot_mock;

const utax_trades_row UTAX_MOCK_PROCESS_YEAR_TRADES[] = {
    {
        .id = 0,
        .quantity = 10.0, .price_per_share = 40.0, .commission = 0.0, .conversion_rate_eur = 1.0,
        .trade_year = 0,
        .broker = "REVO",
        .trade_datetime = "2022-12-01 10:00",
        .type = "BUY",
        .ticker = "WFC",
        .country = "US",
        .currency = "USD"
    },
    {
        .id = 0,
        .quantity = 10.0, .price_per_share = 45.0, .commission = 1.0, .conversion_rate_eur = 1.0,
        .trade_year = 0,
        .broker = "IKBR",
        .trade_datetime = "2023-06-01 10:00",
        .type = "SELL",
        .ticker = "WFC",
        .country = "US",
        .currency = "USD"
    },
    {
        .id = 0,
        .quantity = 8.0, .price_per_share = 100.0, .commission = 8.0, .conversion_rate_eur = 1.0,
        .trade_year = 0,
        .broker = "IKBR",
        .trade_datetime = "2024-01-10 10:00",
        .type = "BUY",
        .ticker = "ABC",
        .country = "US",
        .currency = "USD"
    },
    {
        .id = 0,
        .quantity = 5.0, .price_per_share = 120.0, .commission = 5.0, .conversion_rate_eur = 1.0,
        .trade_year = 0,
        .broker = "IKBR",
        .trade_datetime = "2025-02-01 10:00",
        .type = "BUY",
        .ticker = "ABC",
        .country = "US",
        .currency = "USD"
    },
    {
        .id = 0,
        .quantity = 2.0, .price_per_share = 130.0, .commission = 0.0, .conversion_rate_eur = 1.0,
        .trade_year = 0,
        .broker = "IKBR",
        .trade_datetime = "2025-04-01 10:00",
        .type = "BUY",
        .ticker = "ABC",
        .country = "US",
        .currency = "USD"
    },
    {
        .id = 0,
        .quantity = 12.0, .price_per_share = 150.0, .commission = 12.0, .conversion_rate_eur = 1.0,
        .trade_year = 0,
        .broker = "IKBR",
        .trade_datetime = "2025-06-01 10:00",
        .type = "SELL",
        .ticker = "ABC",
        .country = "US",
        .currency = "USD"
    },
    {
        .id = 0,
        .quantity = 1.0, .price_per_share = 90.0, .commission = 0.0, .conversion_rate_eur = 1.0,
        .trade_year = 0,
        .broker = "IKBR",
        .trade_datetime = "2024-12-15 10:00",
        .type = "BUY",
        .ticker = "OLD",
        .country = "US",
        .currency = "USD"
    }
};

const utax_process_year_snapshot_mock UTAX_MOCK_PROCESS_YEAR_SNAPSHOTS[] = {
    {
        .row = {
            .lot_id = 0,
            .acq_trade_id = 0,
            .qty_remaining = 10.0,
            .cost_per_share_eur = 40.0,
            .acq_commission_eur = 0.0,
            .tax_year = 2022,
            .broker = "REVO",
            .ticker = "WFC",
            .acq_datetime = "2022-12-01 10:00",
            .country = "US"
        },
        .acq_trade_datetime_key = "2022-12-01 10:00"
    },
    {
        .row = {
            .lot_id = 0,
            .acq_trade_id = 0,
            .qty_remaining = 4.0,
            .cost_per_share_eur = 100.0,
            .acq_commission_eur = 2.0,
            .tax_year = 2024,
            .broker = "IKBR",
            .ticker = "ABC",
            .acq_datetime = "2024-01-10 10:00",
            .country = "US"
        },
        .acq_trade_datetime_key = "2024-01-10 10:00"
    },
    {
        .row = {
            .lot_id = 0,
            .acq_trade_id = 0,
            .qty_remaining = 1.0,
            .cost_per_share_eur = 90.0,
            .acq_commission_eur = 0.0,
            .tax_year = 2025,
            .broker = "IKBR",
            .ticker = "OLD",
            .acq_datetime = "2024-12-15 10:00",
            .country = "US"
        },
        .acq_trade_datetime_key = "2024-12-15 10:00"
    }
};

const utax_corporate_actions_row UTAX_MOCK_PROCESS_YEAR_ACTIONS[] = {
    {
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
    },
    {
        .action_id = 0,
        .from_qty = 1.0,
        .to_qty = 2.0,
        .ratio = 0.0,
        .action_year = 0,
        ._pad0 = 0,
        .broker = "IKBR",
        .action_date = "2025-03-01",
        .action_type = "SPLIT",
        .from_ticker = "ABC",
        .to_ticker = ""
    }
};

inline size_t utax_mock_process_year_trades_count(void) {
    return sizeof(UTAX_MOCK_PROCESS_YEAR_TRADES) / sizeof(UTAX_MOCK_PROCESS_YEAR_TRADES[0]);
}

inline size_t utax_mock_process_year_snapshots_count(void) {
    return sizeof(UTAX_MOCK_PROCESS_YEAR_SNAPSHOTS) / sizeof(UTAX_MOCK_PROCESS_YEAR_SNAPSHOTS[0]);
}

inline size_t utax_mock_process_year_actions_count(void) {
    return sizeof(UTAX_MOCK_PROCESS_YEAR_ACTIONS) / sizeof(UTAX_MOCK_PROCESS_YEAR_ACTIONS[0]);
}
