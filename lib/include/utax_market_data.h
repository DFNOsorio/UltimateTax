#pragma once
/** @file utax_market_data.h
 *  @brief Yahoo Finance market data lookup APIs.
 */

#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Dividend-yield resolution status for a quote lookup. */
typedef enum utax_market_dividend_status {
    UTAX_MARKET_DIVIDEND_NOT_REQUESTED = 0,
    UTAX_MARKET_DIVIDEND_AVAILABLE = 1,
    UTAX_MARKET_DIVIDEND_UNAVAILABLE = 2
} utax_market_dividend_status;

/** @brief Result for a single ticker + date lookup. */
typedef struct utax_market_quote {
    char ticker[UTAX_TICKER_MAX];
    char date_yyyy_mm_dd[11];

    double close_price;
    double adjusted_close_price;
    int has_adjusted_close;

    utax_market_dividend_status dividend_status;
    double dividend_amount;
    double dividend_yield_pct;
} utax_market_quote;

/**
 * @brief Parse one Yahoo chart JSON payload for a single-day quote.
 *
 * Expected payload shape is from `/v8/finance/chart/{ticker}` with `interval=1d`.
 * @param ticker Ticker symbol.
 * @param date_yyyy_mm_dd Quote date in `YYYY-MM-DD` format.
 * @param include_dividend_yield Non-zero to request dividend-yield extraction.
 * @param yahoo_chart_json Raw JSON payload from Yahoo chart endpoint.
 * @param out_quote Output quote structure.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_market_data_lookup_yahoo_date_from_json(
    const char *ticker,
    const char *date_yyyy_mm_dd,
    int include_dividend_yield,
    const char *yahoo_chart_json,
    utax_market_quote *out_quote
);

/**
 * @brief Fetch and parse one date-based quote from Yahoo chart endpoint.
 *
 * Uses `period1` and `period2` UNIX seconds (UTC day window) with `interval=1d`.
 * @param ticker Ticker symbol.
 * @param date_yyyy_mm_dd Quote date in `YYYY-MM-DD` format.
 * @param include_dividend_yield Non-zero to request dividend-yield extraction.
 * @param out_quote Output quote structure.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_market_data_lookup_yahoo_date(
    const char *ticker,
    const char *date_yyyy_mm_dd,
    int include_dividend_yield,
    utax_market_quote *out_quote
);

#ifdef __cplusplus
}
#endif
