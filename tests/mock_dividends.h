#pragma once

#include "utax_schema.h"
#include <stddef.h>

/*
  Mock dividends for unit tests.

  Notes:
  - dividend_year is a STORED generated column in DB; keep it 0 in mocks.
  - Use unique dividend_dt values for deterministic ordering.
*/

const utax_dividends_row UTAX_MOCK_DIVIDENDS[] = {
    /* 2024 */
    {
        .dividend_id = 0,
        .per_share = 0.50,
        .total_amount = 50.0,
        .tax = 7.5,
        .conversion_rate_eur = 1.10,

        .dividend_year = 0,

        .broker = "IKBR",
        .ticker = "AAPL",
        .country = "US",
        .dividend_dt = "2024-03-15 12:00",
        .currency = "USD"
    },
    {
        .dividend_id = 0,
        .per_share = 0.25,
        .total_amount = 25.0,
        .tax = 3.0,
        .conversion_rate_eur = 1.15,

        .dividend_year = 0,

        .broker = "IKBR",
        .ticker = "MSFT",
        .country = "US",
        .dividend_dt = "2024-10-10 12:00",
        .currency = "USD"
    },

    /* 2025 */
    {
        .dividend_id = 0,
        .per_share = 0.10,
        .total_amount = 10.0,
        .tax = 1.5,
        .conversion_rate_eur = 1.05,

        .dividend_year = 0,

        .broker = "REVOLUT",
        .ticker = "AAPL",
        .country = "US",
        .dividend_dt = "2025-05-20 12:00",
        .currency = "USD"
    },
    {
        .dividend_id = 0,
        .per_share = 0.30,
        .total_amount = 30.0,
        .tax = 4.5,
        .conversion_rate_eur = 1.00,

        .dividend_year = 0,

        .broker = "REVOLUT",
        .ticker = "TSLA",
        .country = "US",
        .dividend_dt = "2025-12-01 12:00",
        .currency = "USD"
    },

    /* 2026 */
    {
        .dividend_id = 0,
        .per_share = 0.40,
        .total_amount = 40.0,
        .tax = 6.0,
        .conversion_rate_eur = 1.20,

        .dividend_year = 0,

        .broker = "IKBR",
        .ticker = "AAPL",
        .country = "US",
        .dividend_dt = "2026-06-01 12:00",
        .currency = "USD"
    }
};

inline size_t utax_mock_dividends_count(void) {
    return sizeof(UTAX_MOCK_DIVIDENDS) / sizeof(UTAX_MOCK_DIVIDENDS[0]);
}
