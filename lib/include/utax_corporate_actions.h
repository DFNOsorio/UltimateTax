#pragma once
/** @file utax_corporate_actions.h
 *  @brief Corporate actions CSV parsing and database CRUD/query APIs.
 */

#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Filter options for listing and counting corporate action rows. */
typedef struct utax_corporate_actions_filter {
    int has_year;
    int year;
    utax_year_mode year_mode; /* UTAX_YEAR_EXACT / UTAX_YEAR_UP_TO */

    int has_broker;
    char broker[UTAX_BROKER_MAX];

    int has_from_ticker;
    char from_ticker[UTAX_TICKER_MAX];

    /* optional pagination (default: read all) */
    int has_limit;
    int limit;

    int has_offset;
    int offset;
} utax_corporate_actions_filter;


/* CRUD */
/** @brief Inserts one corporate action row. */
UTAX_API utax_rc utax_corporate_actions_insert(utax_db_t *db, const utax_corporate_actions_row *row, long long *out_action_id);
/** @brief Inserts many corporate action rows. */
UTAX_API utax_rc utax_corporate_actions_insert_many(utax_db_t *db,
                                                    utax_corporate_actions_row *rows,
                                                    size_t n,
                                                    size_t *out_inserted);

/** @brief Updates a corporate action row by primary key. */
UTAX_API utax_rc utax_corporate_actions_update_by_id(utax_db_t *db, long long action_id, const utax_corporate_actions_row *row);
/** @brief Deletes a corporate action row by primary key. */
UTAX_API utax_rc utax_corporate_actions_delete_by_id(utax_db_t *db, long long action_id);

/* Counts */
/** @brief Counts all corporate action rows. */
UTAX_API utax_rc utax_corporate_actions_count_total(utax_db_t *db, long long *out_count);
/* ignores pagination */
/** @brief Counts rows matching filters, ignoring pagination fields. */
UTAX_API utax_rc utax_corporate_actions_count_filtered(utax_db_t *db, const utax_corporate_actions_filter *f, long long *out_count);
/* applies pagination */
/** @brief Counts rows matching filters with pagination applied. */
UTAX_API utax_rc utax_corporate_actions_count_page(utax_db_t *db, const utax_corporate_actions_filter *f, long long *out_count);

/* Query rows (paged, preallocated array) */
/** @brief Fetches filtered corporate action rows into a preallocated output buffer. */
UTAX_API utax_rc utax_corporate_actions_get_filtered(utax_db_t *db,
                                                     const utax_corporate_actions_filter *f,
                                                     utax_corporate_actions_row *out_rows,
                                                     size_t out_cap,
                                                     size_t *out_count,
                                                     size_t *out_required);

/** @brief Parses corporate action rows from a CSV file into a dynamic array. */
UTAX_API utax_rc utax_corporate_actions_parse_csv_file(const char *path,
                                                       utax_corporate_actions_row **inout_rows,
                                                       size_t *inout_total_elems);

/** @brief Frees rows allocated by CSV parsing helpers. */
UTAX_API void utax_corporate_actions_free_rows(utax_corporate_actions_row **inout_rows,
                                               size_t *inout_total_elems);

/* Batch insert from dynamic array (transaction + reused statement) */
/** @brief Inserts a dynamic array of corporate action rows in one batch. */
UTAX_API utax_rc utax_corporate_actions_insert_many_array(utax_db_t *db,
                                                         utax_corporate_actions_row *rows,
                                                         size_t n,
                                                         size_t *out_inserted);

/* parse -> insert -> free (always frees rows) */
/** @brief Parses a CSV file, inserts all rows, and frees temporary storage. */
UTAX_API utax_rc utax_corporate_actions_insert_many_from_csv_file(utax_db_t *db,
                                                                  const char *path,
                                                                  size_t *out_inserted);

#ifdef __cplusplus
}
#endif
