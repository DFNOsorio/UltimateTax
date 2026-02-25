#pragma once
#include <stdint.h>
#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

UTAX_API utax_rc process_year_trades(
    utax_db_t *db,
    uint16_t year,
    utax_fifo_realized_row **out_rows,
    size_t *out_count
);

UTAX_API void process_year_free_realized_rows(
    utax_fifo_realized_row **inout_rows,
    size_t *inout_count
);

typedef struct utax_dividends_country_total_row {
    char country[UTAX_COUNTRY_MAX];
    double gross_amount_eur;
    double taxes_eur;
    double total_eur;
} utax_dividends_country_total_row;

UTAX_API utax_rc process_year_dividends_country_totals(
    utax_db_t *db,
    uint16_t year,
    utax_dividends_country_total_row **out_rows,
    size_t *out_count
);

UTAX_API void process_year_free_dividends_country_total_rows(
    utax_dividends_country_total_row **inout_rows,
    size_t *inout_count
);

#ifdef __cplusplus
}
#endif
