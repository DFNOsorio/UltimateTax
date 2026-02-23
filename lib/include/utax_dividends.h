#pragma once
#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct utax_dividends_filter {
    int has_year;
    int year;
    utax_year_mode year_mode; /* EXACT or UP_TO against dividend_year */

    int has_broker;
    char broker[UTAX_BROKER_MAX];

    int has_ticker;
    char ticker[UTAX_TICKER_MAX];

    int has_country;
    char country[UTAX_COUNTRY_MAX];

    int has_currency;
    char currency[UTAX_CCY_MAX];

    int has_limit;
    int limit;

    int has_offset;
    int offset;
} utax_dividends_filter;

/* CRUD */
UTAX_API utax_rc utax_dividends_insert(utax_db_t *db, const utax_dividends_row *row, long long *out_id);
UTAX_API utax_rc utax_dividends_insert_many(utax_db_t *db,
                                            utax_dividends_row *rows,
                                            size_t n,
                                            size_t *out_inserted);
UTAX_API utax_rc utax_dividends_update_by_id(utax_db_t *db, long long id, const utax_dividends_row *row);
UTAX_API utax_rc utax_dividends_delete_by_id(utax_db_t *db, long long id);

/* Counts */
UTAX_API utax_rc utax_dividends_count_total(utax_db_t *db, long long *out_count);
UTAX_API utax_rc utax_dividends_count_filtered(utax_db_t *db, const utax_dividends_filter *f, long long *out_count); /* ignores pagination */
UTAX_API utax_rc utax_dividends_count_page(utax_db_t *db, const utax_dividends_filter *f, long long *out_count);     /* applies pagination */

/* Query rows (paged)
   - Preallocated output array.
   - Capacity check against page size.
*/
UTAX_API utax_rc utax_dividends_get_filtered(utax_db_t *db,
                                             const utax_dividends_filter *f,
                                             utax_dividends_row *out_rows,
                                             size_t out_cap,
                                             size_t *out_count,
                                             size_t *out_required);

#ifdef __cplusplus
}
#endif
