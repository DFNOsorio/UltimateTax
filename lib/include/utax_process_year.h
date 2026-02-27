#pragma once
/**
 * @file utax_process_year.h
 * @brief Year-based processing APIs for realized trades and dividend totals.
 */
#include <stdint.h>
#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Process trades for a year and broker into realized FIFO rows. */
UTAX_API utax_rc process_year_trades(
    utax_db_t *db,
    uint16_t year,
    const char *broker,
    utax_fifo_realized_row **out_rows,
    size_t *out_count
);

/** @brief Free rows allocated by @ref process_year_trades. */
UTAX_API void process_year_free_realized_rows(
    utax_fifo_realized_row **inout_rows,
    size_t *inout_count
);

/** @brief Aggregated dividend totals per country. */
typedef struct utax_dividends_country_total_row {
    char country[UTAX_COUNTRY_MAX];
    double gross_amount_eur;
    double taxes_eur;
    double total_eur;
} utax_dividends_country_total_row;

/** @brief Compute per-country dividend totals for a year and broker. */
UTAX_API utax_rc process_year_dividends_country_totals(
    utax_db_t *db,
    uint16_t year,
    const char *broker,
    utax_dividends_country_total_row **out_rows,
    size_t *out_count
);

/** @brief Free rows allocated by @ref process_year_dividends_country_totals. */
UTAX_API void process_year_free_dividends_country_total_rows(
    utax_dividends_country_total_row **inout_rows,
    size_t *inout_count
);

#ifdef __cplusplus
}
#endif
