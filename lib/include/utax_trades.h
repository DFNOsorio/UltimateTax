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

/**
 * @brief Parses a CSV file and appends parsed trade rows to a dynamic array.
 * @param path CSV file path.
 * @param inout_rows In/out pointer to row-array pointer; may be reallocated.
 * @param inout_total_elems In/out total element count for @p inout_rows.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_trades_parse_csv_file(
    const char *path,
    utax_trades_row **inout_rows,
    size_t *inout_total_elems
);

/**
 * @brief Frees a trade-row array allocated by parsing/query helpers.
 * @param inout_rows In/out pointer to row-array pointer; set to NULL on return.
 * @param inout_total_elems In/out pointer to element count; set to 0 on return.
 */
UTAX_API void utax_trades_free_rows(
    utax_trades_row **inout_rows,
    size_t *inout_total_elems
);

/* CRUD */
/**
 * @brief Inserts one trade row.
 * @param db Open database handle.
 * @param row Row payload to insert.
 * @param out_id Optional output for inserted row id.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_trades_insert(utax_db_t *db, const utax_trades_row *row, long long *out_id);
/**
 * @brief Inserts multiple trade rows.
 * @param db Open database handle.
 * @param rows Rows to insert.
 * @param n Number of elements in @p rows.
 * @param out_inserted Optional output for inserted row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_trades_insert_many(utax_db_t *db,
                                         utax_trades_row *rows,
                                         size_t n,
                                         size_t *out_inserted);
/**
 * @brief Inserts multiple trade rows from an in-memory array.
 * @param db Open database handle.
 * @param rows Rows to insert.
 * @param n Number of elements in @p rows.
 * @param out_inserted Optional output for inserted row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_trades_insert_many_array(
    utax_db_t *db,
    utax_trades_row *rows,
    size_t n,
    size_t *out_inserted
);
/**
 * @brief Parses and inserts trade rows from a CSV file.
 * @param db Open database handle.
 * @param path CSV file path.
 * @param out_inserted Optional output for inserted row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_trades_insert_many_from_csv_file(
    utax_db_t *db,
    const char *path,
    size_t *out_inserted
);
/**
 * @brief Updates a trade row by primary key id.
 * @param db Open database handle.
 * @param id Primary key of the row to update.
 * @param row Replacement row data.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_trades_update_by_id(utax_db_t *db, long long id, const utax_trades_row *row);
/**
 * @brief Deletes a trade row by primary key id.
 * @param db Open database handle.
 * @param id Primary key of the row to delete.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_trades_delete_by_id(utax_db_t *db, long long id);

/* Counts */
/**
 * @brief Counts all trades.
 * @param db Open database handle.
 * @param out_count Output total row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_trades_count_total(utax_db_t *db, long long *out_count);
/**
 * @brief Counts trades matching a filter.
 * @param db Open database handle.
 * @param f Optional filter criteria.
 * @param out_count Output matching row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_trades_count_filtered(utax_db_t *db, const utax_trades_filter *f, long long *out_count);
/**
 * @brief Counts trades in the current filter page.
 * @param db Open database handle.
 * @param f Optional filter criteria including pagination.
 * @param out_count Output paged row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_trades_count_page(utax_db_t *db, const utax_trades_filter *f, long long *out_count);

/* Query rows
   - Writes up to out_cap rows into out_rows.
   - If results > out_cap, returns UTAX_ERR_NO_SPACE and writes required into out_required (if non-NULL).
   - On success, out_count receives the number of rows written.
*/
/**
 * @brief Fetches paged trade rows matching a filter.
 * @param db Open database handle.
 * @param f Optional filter criteria including pagination.
 * @param out_rows Output buffer for fetched rows.
 * @param out_cap Capacity of @p out_rows in elements.
 * @param out_count Output number of written rows.
 * @param out_required Optional output required capacity when @p out_cap is insufficient.
 * @return @ref UTAX_OK on success, otherwise an error code.
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
