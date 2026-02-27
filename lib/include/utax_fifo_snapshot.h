#pragma once
/** @file utax_fifo_snapshot.h
 *  @brief FIFO snapshot-lot database CRUD and query APIs.
 */

#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Filter options for listing and counting FIFO snapshot rows. */
typedef struct utax_fifo_snapshot_filter {
    int has_year;
    int year;
    utax_year_mode year_mode; /* applied to tax_year */

    int has_broker;
    char broker[UTAX_BROKER_MAX];

    int has_ticker;
    char ticker[UTAX_TICKER_MAX];

    int has_country;
    char country[UTAX_COUNTRY_MAX];

    int has_limit;
    int limit;

    int has_offset;
    int offset;
} utax_fifo_snapshot_filter;

/* CRUD */
/** @brief Inserts one FIFO snapshot row. */
UTAX_API utax_rc utax_fifo_snapshot_insert(utax_db_t *db, const utax_fifo_snapshot_row *row, long long *out_lot_id);

/* NEW: Batch insert
   - Inserts 'n' rows in ONE transaction with ONE prepared statement reused.
   - On success: returns UTAX_OK, sets rows[i].lot_id for all rows, sets *out_inserted = n.
   - On failure: ROLLBACK; returns UTAX_ERR_SQLITE (or another error), sets *out_inserted = number of rows processed before failure.
     NOTE: because of rollback, the DB will contain NONE of the inserted rows.
*/
/** @brief Inserts many FIFO snapshot rows in a single transaction. */
UTAX_API utax_rc utax_fifo_snapshot_insert_many(utax_db_t *db,
                                                utax_fifo_snapshot_row *rows,
                                                size_t n,
                                                size_t *out_inserted);

/** @brief Updates a FIFO snapshot row by primary key. */
UTAX_API utax_rc utax_fifo_snapshot_update_by_id(utax_db_t *db, long long lot_id, const utax_fifo_snapshot_row *row);
/** @brief Deletes a FIFO snapshot row by primary key. */
UTAX_API utax_rc utax_fifo_snapshot_delete_by_id(utax_db_t *db, long long lot_id);

/* Counts */
/** @brief Counts all FIFO snapshot rows. */
UTAX_API utax_rc utax_fifo_snapshot_count_total(utax_db_t *db, long long *out_count);
/** @brief Counts rows matching filters, ignoring pagination fields. */
UTAX_API utax_rc utax_fifo_snapshot_count_filtered(utax_db_t *db, const utax_fifo_snapshot_filter *f, long long *out_count); /* ignores pagination */
/** @brief Counts rows matching filters with pagination applied. */
UTAX_API utax_rc utax_fifo_snapshot_count_page(utax_db_t *db, const utax_fifo_snapshot_filter *f, long long *out_count);     /* applies pagination */

/* Query rows (paged) */
/** @brief Fetches filtered FIFO snapshot rows into a preallocated output buffer. */
UTAX_API utax_rc utax_fifo_snapshot_get_filtered(utax_db_t *db,
                                                 const utax_fifo_snapshot_filter *f,
                                                 utax_fifo_snapshot_row *out_rows,
                                                 size_t out_cap,
                                                 size_t *out_count,
                                                 size_t *out_required);

#ifdef __cplusplus
}
#endif
