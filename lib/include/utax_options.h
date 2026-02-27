#pragma once
/** @file utax_options.h
 *  @brief Stock options CSV parsing and database CRUD/query APIs.
 */

#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Filter options for listing and counting stock options rows. */
typedef struct utax_options_filter {
    int has_year;                 /**< Non-zero to apply year filtering. */
    int year;                     /**< Year value used when @ref has_year is set. */
    utax_year_mode year_mode;     /**< Year-match mode: exact or up-to. */

    int has_broker;               /**< Non-zero to filter by broker. */
    char broker[UTAX_BROKER_MAX]; /**< Broker value to match when @ref has_broker is set. */

    int has_ticker;               /**< Non-zero to filter by ticker. */
    char ticker[UTAX_TICKER_MAX]; /**< Ticker value to match when @ref has_ticker is set. */

    int has_limit;                /**< Non-zero to apply LIMIT pagination. */
    int limit;                    /**< Maximum rows to return when @ref has_limit is set. */

    int has_offset;               /**< Non-zero to apply OFFSET pagination. */
    int offset;                   /**< Row offset when @ref has_offset is set. */
} utax_options_filter;

/**
 * @brief Parses stock options rows from a CSV file into a dynamic array.
 * @param path CSV file path.
 * @param inout_rows In/out pointer to dynamic row array; may be reallocated and appended to.
 * @param inout_total_elems In/out total element count in @p inout_rows.
 * @return @ref utax_rc status code.
 */
UTAX_API utax_rc utax_options_parse_csv_file(
    const char *path,
    utax_options_row **inout_rows,
    size_t *inout_total_elems
);

/**
 * @brief Frees rows allocated by CSV parsing helpers.
 * @param inout_rows In/out pointer to array pointer to free; set to NULL on return.
 * @param inout_total_elems In/out pointer to element count; set to 0 on return.
 */
UTAX_API void utax_options_free_rows(utax_options_row **inout_rows, size_t *inout_total_elems);

/* CRUD */
/**
 * @brief Inserts one stock options row.
 * @param db Open database handle.
 * @param row Row data to insert.
 * @param out_id Optional output for inserted row id.
 * @return @ref utax_rc status code.
 */
UTAX_API utax_rc utax_options_insert(utax_db_t *db, const utax_options_row *row, long long *out_id);
/**
 * @brief Inserts many stock options rows.
 * @param db Open database handle.
 * @param rows Input rows to insert.
 * @param n Number of rows in @p rows.
 * @param out_inserted Optional output for number of successfully inserted rows.
 * @return @ref utax_rc status code.
 */
UTAX_API utax_rc utax_options_insert_many(utax_db_t *db, utax_options_row *rows, size_t n, size_t *out_inserted);
/**
 * @brief Inserts a dynamic array of stock options rows in one batch.
 * @param db Open database handle.
 * @param rows Input rows to insert.
 * @param n Number of rows in @p rows.
 * @param out_inserted Optional output for number of successfully inserted rows.
 * @return @ref utax_rc status code.
 */
UTAX_API utax_rc utax_options_insert_many_array(utax_db_t *db, utax_options_row *rows, size_t n, size_t *out_inserted);
/**
 * @brief Parses a CSV file, inserts all rows, and frees temporary storage.
 * @param db Open database handle.
 * @param path CSV file path.
 * @param out_inserted Optional output for number of successfully inserted rows.
 * @return @ref utax_rc status code.
 */
UTAX_API utax_rc utax_options_insert_many_from_csv_file(utax_db_t *db, const char *path, size_t *out_inserted);
/**
 * @brief Updates a stock options row by primary key.
 * @param db Open database handle.
 * @param id Primary key id of the row to update.
 * @param row Replacement row data.
 * @return @ref utax_rc status code.
 */
UTAX_API utax_rc utax_options_update_by_id(utax_db_t *db, long long id, const utax_options_row *row);
/**
 * @brief Deletes a stock options row by primary key.
 * @param db Open database handle.
 * @param id Primary key id of the row to delete.
 * @return @ref utax_rc status code.
 */
UTAX_API utax_rc utax_options_delete_by_id(utax_db_t *db, long long id);

/* Counts */
/**
 * @brief Counts all stock options rows.
 * @param db Open database handle.
 * @param out_count Output total row count.
 * @return @ref utax_rc status code.
 */
UTAX_API utax_rc utax_options_count_total(utax_db_t *db, long long *out_count);
/**
 * @brief Counts rows matching filters, ignoring pagination fields.
 * @param db Open database handle.
 * @param f Optional filter criteria.
 * @param out_count Output matching row count.
 * @return @ref utax_rc status code.
 */
UTAX_API utax_rc utax_options_count_filtered(utax_db_t *db, const utax_options_filter *f, long long *out_count);
/**
 * @brief Counts rows matching filters with pagination applied.
 * @param db Open database handle.
 * @param f Optional filter criteria.
 * @param out_count Output count after pagination.
 * @return @ref utax_rc status code.
 */
UTAX_API utax_rc utax_options_count_page(utax_db_t *db, const utax_options_filter *f, long long *out_count);

/**
 * @brief Fetches filtered stock options rows into a preallocated output buffer.
 * @param db Open database handle.
 * @param f Optional filter criteria (including pagination).
 * @param out_rows Output buffer for result rows.
 * @param out_cap Capacity of @p out_rows in elements.
 * @param out_count Output number of rows written to @p out_rows.
 * @param out_required Optional output required capacity when @p out_cap is insufficient.
 * @return @ref utax_rc status code.
 */
UTAX_API utax_rc utax_options_get_filtered(utax_db_t *db,
                                           const utax_options_filter *f,
                                           utax_options_row *out_rows,
                                           size_t out_cap,
                                           size_t *out_count,
                                           size_t *out_required);

#ifdef __cplusplus
}
#endif
