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
/**
 * @brief Inserts one FIFO snapshot row.
 * @param db Open database handle.
 * @param row Row payload to insert.
 * @param out_lot_id Optional output for inserted row id.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_insert(utax_db_t *db, const utax_fifo_snapshot_row *row, long long *out_lot_id);

/* NEW: Batch insert
   - Inserts 'n' rows in ONE transaction with ONE prepared statement reused.
   - On success: returns UTAX_OK, sets rows[i].lot_id for all rows, sets *out_inserted = n.
   - On failure: ROLLBACK; returns UTAX_ERR_SQLITE (or another error), sets *out_inserted = number of rows processed before failure.
     NOTE: because of rollback, the DB will contain NONE of the inserted rows.
*/
/**
 * @brief Inserts many FIFO snapshot rows in a single transaction.
 * @param db Open database handle.
 * @param rows Rows to insert.
 * @param n Number of elements in @p rows.
 * @param out_inserted Optional output for processed row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_insert_many(utax_db_t *db,
                                                utax_fifo_snapshot_row *rows,
                                                size_t n,
                                                size_t *out_inserted);

/**
 * @brief Updates a FIFO snapshot row by primary key.
 * @param db Open database handle.
 * @param lot_id Primary key of the row to update.
 * @param row Replacement row data.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_update_by_id(utax_db_t *db, long long lot_id, const utax_fifo_snapshot_row *row);
/**
 * @brief Deletes a FIFO snapshot row by primary key.
 * @param db Open database handle.
 * @param lot_id Primary key of the row to delete.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_delete_by_id(utax_db_t *db, long long lot_id);

/* Counts */
/**
 * @brief Counts all FIFO snapshot rows.
 * @param db Open database handle.
 * @param out_count Output total row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_count_total(utax_db_t *db, long long *out_count);
/**
 * @brief Counts rows matching filters, ignoring pagination fields.
 * @param db Open database handle.
 * @param f Optional filter criteria.
 * @param out_count Output matching row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_count_filtered(utax_db_t *db, const utax_fifo_snapshot_filter *f, long long *out_count); /* ignores pagination */
/**
 * @brief Counts rows matching filters with pagination applied.
 * @param db Open database handle.
 * @param f Optional filter criteria including pagination.
 * @param out_count Output paged row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_count_page(utax_db_t *db, const utax_fifo_snapshot_filter *f, long long *out_count);     /* applies pagination */

/* Query rows (paged) */
/**
 * @brief Fetches filtered FIFO snapshot rows into a preallocated output buffer.
 * @param db Open database handle.
 * @param f Optional filter criteria including pagination.
 * @param out_rows Output buffer for fetched rows.
 * @param out_cap Capacity of @p out_rows in elements.
 * @param out_count Output number of written rows.
 * @param out_required Optional output required capacity when @p out_cap is insufficient.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_get_filtered(utax_db_t *db,
                                                 const utax_fifo_snapshot_filter *f,
                                                 utax_fifo_snapshot_row *out_rows,
                                                 size_t out_cap,
                                                 size_t *out_count,
                                                 size_t *out_required);

/* Market-data driven price refresh APIs (phase-2).
   Stored quote fields are:
   - original quote price (`last_updated_stock_price`)
   - quote currency (`last_updated_stock_currency`)
   - FX rate to EUR (`last_updated_stock_conversion_rate_eur`)
*/
/**
 * @brief Updates one snapshot lot quote fields by lot id.
 *        If market data is unavailable on the requested date, lookup falls back up to 3 days back.
 *        Stored `last_price_update_date` is the actual quote date used.
 * @param db Open database handle.
 * @param lot_id Snapshot lot identifier.
 * @param date_yyyy_mm_dd Quote date in `YYYY-MM-DD` format.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_update_price_by_lot_id(utax_db_t *db,
                                                           long long lot_id,
                                                           const char *date_yyyy_mm_dd);

/**
 * @brief Updates all snapshot lots for a ticker using market data for one date.
 *        If market data is unavailable on the requested date, lookup falls back up to 3 days back.
 *        Stored `last_price_update_date` is the actual quote date used.
 * @param db Open database handle.
 * @param ticker Ticker symbol.
 * @param date_yyyy_mm_dd Quote date in `YYYY-MM-DD` format.
 * @param out_updated Optional output count of rows successfully updated.
 * @param out_unavailable Optional output count of rows with missing market data.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_update_prices_by_ticker(utax_db_t *db,
                                                            const char *ticker,
                                                            const char *date_yyyy_mm_dd,
                                                            size_t *out_updated,
                                                            size_t *out_unavailable);

/**
 * @brief Mass-updates snapshot lots selected by filter/pagination.
 *        If market data is unavailable on the requested date, lookup falls back up to 3 days back.
 *        Stored `last_price_update_date` is the actual quote date used.
 * @param db Open database handle.
 * @param date_yyyy_mm_dd Quote date in `YYYY-MM-DD` format.
 * @param f Optional filter criteria including pagination.
 * @param out_processed Optional output count of processed rows.
 * @param out_updated Optional output count of rows successfully updated.
 * @param out_unavailable Optional output count of rows with missing market data.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_update_prices_paged(utax_db_t *db,
                                                        const char *date_yyyy_mm_dd,
                                                        const utax_fifo_snapshot_filter *f,
                                                        size_t *out_processed,
                                                        size_t *out_updated,
                                                        size_t *out_unavailable);

#ifdef __cplusplus
}
#endif
