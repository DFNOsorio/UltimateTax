#pragma once
/**
 * @file utax_trades.h
 * @brief CSV parsing plus CRUD/count/query API for trade rows.
 */
#include "utax_db.h"
#include "utax_schema.h"  /* for utax_trades_row + max string sizes */

#ifdef __cplusplus
extern "C" {
#endif


/** @brief Filter criteria for querying trades. */
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

/** @brief Parse a CSV file and append parsed trade rows to a dynamic array. */
UTAX_API utax_rc utax_trades_parse_csv_file(
    const char *path,
    utax_trades_row **inout_rows,
    size_t *inout_total_elems
);

/** @brief Free a trade-row array allocated by parsing/query helpers. */
UTAX_API void utax_trades_free_rows(
    utax_trades_row **inout_rows,
    size_t *inout_total_elems
);

/* CRUD */
/** @brief Insert one trade row. */
UTAX_API utax_rc utax_trades_insert(utax_db_t *db, const utax_trades_row *row, long long *out_id);
/** @brief Insert multiple trade rows. */
UTAX_API utax_rc utax_trades_insert_many(utax_db_t *db,
                                         utax_trades_row *rows,
                                         size_t n,
                                         size_t *out_inserted);
/** @brief Insert multiple trade rows from an in-memory array. */
UTAX_API utax_rc utax_trades_insert_many_array(
    utax_db_t *db,
    utax_trades_row *rows,
    size_t n,
    size_t *out_inserted
);
/** @brief Parse and insert trade rows from a CSV file. */
UTAX_API utax_rc utax_trades_insert_many_from_csv_file(
    utax_db_t *db,
    const char *path,
    size_t *out_inserted
);
/** @brief Update a trade row by primary key id. */
UTAX_API utax_rc utax_trades_update_by_id(utax_db_t *db, long long id, const utax_trades_row *row);
/** @brief Delete a trade row by primary key id. */
UTAX_API utax_rc utax_trades_delete_by_id(utax_db_t *db, long long id);

/* Counts */
/** @brief Count all trades. */
UTAX_API utax_rc utax_trades_count_total(utax_db_t *db, long long *out_count);
/** @brief Count trades matching a filter. */
UTAX_API utax_rc utax_trades_count_filtered(utax_db_t *db, const utax_trades_filter *f, long long *out_count);
/** @brief Count trades in the current filter page. */
UTAX_API utax_rc utax_trades_count_page(utax_db_t *db, const utax_trades_filter *f, long long *out_count);

/* Query rows
   - Writes up to out_cap rows into out_rows.
   - If results > out_cap, returns UTAX_ERR_NO_SPACE and writes required into out_required (if non-NULL).
   - On success, out_count receives the number of rows written.
*/
/** @brief Fetch paged trade rows matching a filter. */
UTAX_API utax_rc utax_trades_get_filtered(utax_db_t *db,
                                          const utax_trades_filter *f,
                                          utax_trades_row *out_rows,
                                          size_t out_cap,
                                          size_t *out_count,
                                          size_t *out_required);

#ifdef __cplusplus
}
#endif
