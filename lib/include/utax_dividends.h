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

/** @brief Parses dividend rows from a CSV file into a dynamic array. */
UTAX_API utax_rc utax_dividends_parse_csv_file(
    const char *path,
    utax_dividends_row **inout_rows,
    size_t *inout_total_elems
);

/** @brief Frees rows allocated by CSV parsing helpers. */
UTAX_API void utax_dividends_free_rows(utax_dividends_row **inout_rows, size_t *inout_total_elems);

/* CRUD */
/** @brief Inserts one dividend row. */
UTAX_API utax_rc utax_dividends_insert(utax_db_t *db, const utax_dividends_row *row, long long *out_id);
/** @brief Inserts many dividend rows. */
UTAX_API utax_rc utax_dividends_insert_many(utax_db_t *db,
                                            utax_dividends_row *rows,
                                            size_t n,
                                            size_t *out_inserted);
/** @brief Inserts a dynamic array of dividend rows in one batch. */
UTAX_API utax_rc utax_dividends_insert_many_array(
    utax_db_t *db,
    utax_dividends_row *rows,
    size_t n,
    size_t *out_inserted
);
/** @brief Parses a CSV file, inserts all rows, and frees temporary storage. */
UTAX_API utax_rc utax_dividends_insert_many_from_csv_file(
    utax_db_t *db,
    const char *path,
    size_t *out_inserted
);
/** @brief Updates a dividend row by primary key. */
UTAX_API utax_rc utax_dividends_update_by_id(utax_db_t *db, long long id, const utax_dividends_row *row);
/** @brief Deletes a dividend row by primary key. */
UTAX_API utax_rc utax_dividends_delete_by_id(utax_db_t *db, long long id);

/* Counts */
/** @brief Counts all dividend rows. */
UTAX_API utax_rc utax_dividends_count_total(utax_db_t *db, long long *out_count);
/** @brief Counts rows matching filters, ignoring pagination fields. */
UTAX_API utax_rc utax_dividends_count_filtered(utax_db_t *db, const utax_dividends_filter *f, long long *out_count); /* ignores pagination */
/** @brief Counts rows matching filters with pagination applied. */
UTAX_API utax_rc utax_dividends_count_page(utax_db_t *db, const utax_dividends_filter *f, long long *out_count);     /* applies pagination */

/* Query rows (paged)
   - Preallocated output array.
   - Capacity check against page size.
*/
/** @brief Fetches filtered dividend rows into a preallocated output buffer. */
UTAX_API utax_rc utax_dividends_get_filtered(utax_db_t *db,
                                             const utax_dividends_filter *f,
                                             utax_dividends_row *out_rows,
                                             size_t out_cap,
                                             size_t *out_count,
                                             size_t *out_required);



#ifdef __cplusplus
}
#endif
