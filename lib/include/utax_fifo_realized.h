#pragma once
#include "utax_db.h"
#include "utax_schema.h" /* utax_fifo_realized_row + size constants */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct utax_fifo_realized_filter {
    int has_year;
    int year;
    utax_year_mode year_mode; /* applied to tax_year */

    int has_broker;
    char broker[UTAX_BROKER_MAX];

    int has_ticker;
    char ticker[UTAX_TICKER_MAX];

    int has_country;
    char country[UTAX_COUNTRY_MAX];

    int has_sell_trade_id;
    long long sell_trade_id;

    int has_buy_trade_id;
    long long buy_trade_id;

    int has_limit;
    int limit;

    int has_offset;
    int offset;
} utax_fifo_realized_filter;

/* CRUD */
UTAX_API utax_rc utax_fifo_realized_insert(utax_db_t *db, const utax_fifo_realized_row *row, long long *out_realized_id);

/* NEW: Batch insert
   - Inserts n rows in ONE transaction with ONE prepared statement reused.
   - On success: returns UTAX_OK, sets rows[i].realized_id, sets *out_inserted = n.
   - On failure: ROLLBACK; returns error; sets *out_inserted = rows processed before failure.
     NOTE: due to rollback, DB contains NONE of the batch rows.
*/
UTAX_API utax_rc utax_fifo_realized_insert_many(utax_db_t *db,
                                                utax_fifo_realized_row *rows,
                                                size_t n,
                                                size_t *out_inserted);

UTAX_API utax_rc utax_fifo_realized_update_by_id(utax_db_t *db, long long realized_id, const utax_fifo_realized_row *row);
UTAX_API utax_rc utax_fifo_realized_delete_by_id(utax_db_t *db, long long realized_id);

/* Counts */
UTAX_API utax_rc utax_fifo_realized_count_total(utax_db_t *db, long long *out_count);
UTAX_API utax_rc utax_fifo_realized_count_filtered(utax_db_t *db, const utax_fifo_realized_filter *f, long long *out_count); /* ignores pagination */
UTAX_API utax_rc utax_fifo_realized_count_page(utax_db_t *db, const utax_fifo_realized_filter *f, long long *out_count);     /* applies pagination */

/* Query rows (paged)
   - Capacity check is against page size.
*/
UTAX_API utax_rc utax_fifo_realized_get_filtered(utax_db_t *db,
                                                 const utax_fifo_realized_filter *f,
                                                 utax_fifo_realized_row *out_rows,
                                                 size_t out_cap,
                                                 size_t *out_count,
                                                 size_t *out_required);

#ifdef __cplusplus
}
#endif
