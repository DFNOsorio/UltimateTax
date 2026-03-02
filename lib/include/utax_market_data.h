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
    char currency[UTAX_CCY_MAX];

    double open_price;
    double close_price;
    double adjusted_close_price;
    double conversion_rate_eur;
    int has_open_price;
    int has_adjusted_close;
    int has_conversion_rate_eur;

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
 *        Populates price/currency fields from Yahoo payload.
 *        Does not fetch external FX data, so `has_conversion_rate_eur` remains false.
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
 *        Populates quote price/currency and attempts to fetch FX conversion rate to EUR.
 *        If no stock data exists for the requested date, it retries up to 3 previous days.
 *        If no FX value exists for the quote date, it retries up to 3 previous days.
 *        `out_quote->date_yyyy_mm_dd` is the actual stock quote date used after fallback.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_market_data_lookup_yahoo_date(
    const char *ticker,
    const char *date_yyyy_mm_dd,
    int include_dividend_yield,
    utax_market_quote *out_quote
);

/**
 * @brief Fetch and parse one date-based quote from Yahoo chart endpoint, using forward-day fallback.
 *
 * Tries requested date first, then up to 3 following days.
 * Intended for corporate-action child pricing (e.g., spin-offs) where first tradable quote
 * may appear after the action date.
 */
UTAX_API utax_rc utax_market_data_lookup_yahoo_date_forward(
    const char *ticker,
    const char *date_yyyy_mm_dd,
    int include_dividend_yield,
    utax_market_quote *out_quote
);

#ifdef __cplusplus
}
#endif
