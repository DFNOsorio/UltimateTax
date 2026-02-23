#pragma once

#include "utax_schema.h"
#include <stddef.h>

/*
  Mock trades for unit tests.

  Notes:
  - trade_year is generated in DB; we keep it 0 here.
  - We keep datetimes unique so ordering is deterministic by trade_datetime ASC.
  - Some fields are empty to test defaults in utax_trades_insert/update:
      broker/type/country/currency can be "" to trigger DB-side defaults in your SQL.
*/

const utax_trades_row UTAX_MOCK_TRADES[] = {
    /* 2024 */
    {
        .id = 0,
        .quantity = 10.0, .price_per_share = 100.0, .commission = 1.0, .conversion_rate_eur = 1.10,
        .trade_year = 0,
        .broker = "IKBR",
        .trade_datetime = "2024-01-10 09:30",
        .type = "BUY",
        .ticker = "AAPL",
        .country = "US",
        .currency = "USD"
    },
    {
        .id = 0,
        .quantity = 5.0, .price_per_share = 200.0, .commission = 0.5, .conversion_rate_eur = 1.15,
        .trade_year = 0,
        .broker = "IKBR",
        .trade_datetime = "2024-06-20 15:45",
        .type = "SELL",
        .ticker = "MSFT",
        .country = "US",
        .currency = "USD"
    },

    /* 2025 */
    {
        .id = 0,
        .quantity = 3.0, .price_per_share = 50.0, .commission = 0.0, .conversion_rate_eur = 1.05,
        .trade_year = 0,
        .broker = "REVOLUT",
        .trade_datetime = "2025-02-05 10:00",
        .type = "BUY",
        .ticker = "AAPL",
        .country = "US",
        .currency = "USD"
    },
    {
        /* tests defaults: broker/type/country/currency are empty => should store defaults */
        .id = 0,
        .quantity = 7.0, .price_per_share = 75.0, .commission = 0.0, .conversion_rate_eur = 1.00,
        .trade_year = 0,
        .broker = "",
        .trade_datetime = "2025-11-01 12:15",
        .type = "",
        .ticker = "TSLA",
        .country = "",
        .currency = ""
    },

    /* 2026 */
    {
        .id = 0,
        .quantity = 2.0, .price_per_share = 300.0, .commission = 2.5, .conversion_rate_eur = 1.20,
        .trade_year = 0,
        .broker = "IKBR",
        .trade_datetime = "2026-03-03 09:00",
        .type = "BUY",
        .ticker = "MSFT",
        .country = "US",
        .currency = "USD"
    },
    {
        .id = 0,
        .quantity = 1.0, .price_per_share = 500.0, .commission = 1.0, .conversion_rate_eur = 1.25,
        .trade_year = 0,
        .broker = "REVOLUT",
        .trade_datetime = "2026-12-31 16:00",
        .type = "SELL",
        .ticker = "AAPL",
        .country = "US",
        .currency = "USD"
    }
};

inline size_t utax_mock_trades_count(void) {
    return sizeof(UTAX_MOCK_TRADES) / sizeof(UTAX_MOCK_TRADES[0]);
}
