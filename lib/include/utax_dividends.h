#pragma once
/** @file utax_dividends.h
 *  @brief Dividend CSV parsing and database CRUD/query APIs.
 */

#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Filter options for listing and counting dividend rows. */
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

/**
 * @brief Parses dividend rows from a CSV file into a dynamic array.
 * @param path CSV file path.
 * @param inout_rows In/out pointer to row-array pointer; may be reallocated.
 * @param inout_total_elems In/out element count for @p inout_rows.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_dividends_parse_csv_file(
    const char *path,
    utax_dividends_row **inout_rows,
    size_t *inout_total_elems
);

/**
 * @brief Frees rows allocated by CSV parsing helpers.
 * @param inout_rows In/out pointer to row-array pointer; set to NULL on return.
 * @param inout_total_elems In/out pointer to element count; set to 0 on return.
 */
UTAX_API void utax_dividends_free_rows(utax_dividends_row **inout_rows, size_t *inout_total_elems);

/* CRUD */
/**
 * @brief Inserts one dividend row.
 * @param db Open database handle.
 * @param row Row payload to insert.
 * @param out_id Optional output for inserted row id.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_dividends_insert(utax_db_t *db, const utax_dividends_row *row, long long *out_id);
/**
 * @brief Inserts many dividend rows.
 * @param db Open database handle.
 * @param rows Rows to insert.
 * @param n Number of elements in @p rows.
 * @param out_inserted Optional output for inserted row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_dividends_insert_many(utax_db_t *db,
                                            utax_dividends_row *rows,
                                            size_t n,
                                            size_t *out_inserted);
/**
 * @brief Inserts a dynamic array of dividend rows in one batch.
 * @param db Open database handle.
 * @param rows Rows to insert.
 * @param n Number of elements in @p rows.
 * @param out_inserted Optional output for inserted row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_dividends_insert_many_array(
    utax_db_t *db,
    utax_dividends_row *rows,
    size_t n,
    size_t *out_inserted
);
/**
 * @brief Parses a CSV file, inserts all rows, and frees temporary storage.
 * @param db Open database handle.
 * @param path CSV file path.
 * @param out_inserted Optional output for inserted row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_dividends_insert_many_from_csv_file(
    utax_db_t *db,
    const char *path,
    size_t *out_inserted
);
/**
 * @brief Updates a dividend row by primary key.
 * @param db Open database handle.
 * @param id Primary key of the row to update.
 * @param row Replacement row data.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_dividends_update_by_id(utax_db_t *db, long long id, const utax_dividends_row *row);
/**
 * @brief Deletes a dividend row by primary key.
 * @param db Open database handle.
 * @param id Primary key of the row to delete.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_dividends_delete_by_id(utax_db_t *db, long long id);

/* Counts */
/**
 * @brief Counts all dividend rows.
 * @param db Open database handle.
 * @param out_count Output total row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_dividends_count_total(utax_db_t *db, long long *out_count);
/**
 * @brief Counts rows matching filters, ignoring pagination fields.
 * @param db Open database handle.
 * @param f Optional filter criteria.
 * @param out_count Output matching row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_dividends_count_filtered(utax_db_t *db, const utax_dividends_filter *f, long long *out_count); /* ignores pagination */
/**
 * @brief Counts rows matching filters with pagination applied.
 * @param db Open database handle.
 * @param f Optional filter criteria including pagination.
 * @param out_count Output paged row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_dividends_count_page(utax_db_t *db, const utax_dividends_filter *f, long long *out_count);     /* applies pagination */

/* Query rows (paged)
   - Preallocated output array.
   - Capacity check against page size.
*/
/**
 * @brief Fetches filtered dividend rows into a preallocated output buffer.
 * @param db Open database handle.
 * @param f Optional filter criteria including pagination.
 * @param out_rows Output buffer for fetched rows.
 * @param out_cap Capacity of @p out_rows in elements.
 * @param out_count Output number of written rows.
 * @param out_required Optional output required capacity when @p out_cap is insufficient.
 * @return @ref UTAX_OK on success, otherwise an error code.
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
