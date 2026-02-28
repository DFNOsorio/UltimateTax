#pragma once

#include "utax_schema.h"
#include <stddef.h>

typedef struct utax_fifo_snapshot_mock {
    utax_fifo_snapshot_row row;
    char acq_trade_datetime_key[UTAX_DT_MAX]; /* used to map to inserted trades.id in the test */
} utax_fifo_snapshot_mock;

const utax_fifo_snapshot_mock UTAX_MOCK_FIFO_SNAPSHOTS[] = {
    {
        .row = {
            .lot_id = 0,
            .broker = "IKBR",
            .tax_year = 2024,
            .ticker = "AAPL",
            .acq_trade_id = 0, /* filled in test */
            .acq_datetime = "2024-01-10 09:30",
            .qty_remaining = 10.0,
            .cost_per_share_eur = 90.0,
            .acq_commission_eur = 1.0,
            .last_price_update_date = "2024-01-11 16:00",
            .last_updated_stock_price = 95.5,
            .country = "US"
        },
        .acq_trade_datetime_key = "2024-01-10 09:30"
    },
    {
        .row = {
            .lot_id = 0,
            .broker = "REVOLUT",
            .tax_year = 2025,
            .ticker = "AAPL",
            .acq_trade_id = 0,
            .acq_datetime = "2025-02-05 10:00",
            .qty_remaining = 3.0,
            .cost_per_share_eur = 45.0,
            .acq_commission_eur = 0.0,
            .last_price_update_date = "2025-02-06 16:00",
            .last_updated_stock_price = 48.0,
            .country = "US"
        },
        .acq_trade_datetime_key = "2025-02-05 10:00"
    },
    {
        .row = {
            .lot_id = 0,
            .broker = "IKBR",
            .tax_year = 2026,
            .ticker = "MSFT",
            .acq_trade_id = 0,
            .acq_datetime = "2026-03-03 09:00",
            .qty_remaining = 2.0,
            .cost_per_share_eur = 280.0,
            .acq_commission_eur = 2.5,
            .last_price_update_date = "2026-03-04 16:00",
            .last_updated_stock_price = 300.0,
            .country = "US"
        },
        .acq_trade_datetime_key = "2026-03-03 09:00"
    }
};

inline size_t utax_mock_fifo_snapshots_count(void) {
    return sizeof(UTAX_MOCK_FIFO_SNAPSHOTS) / sizeof(UTAX_MOCK_FIFO_SNAPSHOTS[0]);
}
