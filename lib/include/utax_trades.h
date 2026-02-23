#pragma once
#include "utax_db.h"
#include "utax_schema.h"  /* for utax_trades_row + max string sizes */

#ifdef __cplusplus
extern "C" {
#endif


typedef struct utax_trades_filter {
    int has_year;
    int year;
    utax_year_mode year_mode;

    int has_broker;
    char broker[UTAX_BROKER_MAX];

    int has_ticker;
    char ticker[UTAX_TICKER_MAX];

    int has_type;
    char type[UTAX_TYPE_MAX];

    int has_limit;
    int limit;

    int has_offset;
    int offset;
} utax_trades_filter;

/* CRUD */
UTAX_API utax_rc utax_trades_insert(utax_db_t *db, const utax_trades_row *row, long long *out_id);
UTAX_API utax_rc utax_trades_update_by_id(utax_db_t *db, long long id, const utax_trades_row *row);
UTAX_API utax_rc utax_trades_delete_by_id(utax_db_t *db, long long id);

/* Counts */
UTAX_API utax_rc utax_trades_count_total(utax_db_t *db, long long *out_count);
UTAX_API utax_rc utax_trades_count_filtered(utax_db_t *db, const utax_trades_filter *f, long long *out_count);
UTAX_API utax_rc utax_trades_count_page(utax_db_t *db, const utax_trades_filter *f, long long *out_count);

/* Query rows
   - Writes up to out_cap rows into out_rows.
   - If results > out_cap, returns UTAX_ERR_NO_SPACE and writes required into out_required (if non-NULL).
   - On success, out_count receives the number of rows written.
*/
UTAX_API utax_rc utax_trades_get_filtered(utax_db_t *db,
                                          const utax_trades_filter *f,
                                          utax_trades_row *out_rows,
                                          size_t out_cap,
                                          size_t *out_count,
                                          size_t *out_required);

#ifdef __cplusplus
}
#endif
