#pragma once
#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

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

typedef struct utax_corporate_actions_node {
    utax_corporate_actions_row row;
    struct utax_corporate_actions_node *next;
} utax_corporate_actions_node;


/* CRUD */
UTAX_API utax_rc utax_corporate_actions_insert(utax_db_t *db, const utax_corporate_actions_row *row, long long *out_action_id);
UTAX_API utax_rc utax_corporate_actions_insert_many(utax_db_t *db,
                                                    utax_corporate_actions_row *rows,
                                                    size_t n,
                                                    size_t *out_inserted);

UTAX_API utax_rc utax_corporate_actions_update_by_id(utax_db_t *db, long long action_id, const utax_corporate_actions_row *row);
UTAX_API utax_rc utax_corporate_actions_delete_by_id(utax_db_t *db, long long action_id);

/* Counts */
UTAX_API utax_rc utax_corporate_actions_count_total(utax_db_t *db, long long *out_count);
/* ignores pagination */
UTAX_API utax_rc utax_corporate_actions_count_filtered(utax_db_t *db, const utax_corporate_actions_filter *f, long long *out_count);
/* applies pagination */
UTAX_API utax_rc utax_corporate_actions_count_page(utax_db_t *db, const utax_corporate_actions_filter *f, long long *out_count);

/* Query rows (paged, preallocated array) */
UTAX_API utax_rc utax_corporate_actions_get_filtered(utax_db_t *db,
                                                     const utax_corporate_actions_filter *f,
                                                     utax_corporate_actions_row *out_rows,
                                                     size_t out_cap,
                                                     size_t *out_count,
                                                     size_t *out_required);

UTAX_API utax_rc utax_corporate_actions_parse_csv_file(const char *path,
                                                       utax_corporate_actions_node **inout_head,
                                                       size_t *inout_total_elems);

UTAX_API void utax_corporate_actions_free_list(utax_corporate_actions_node **inout_head,
                                               size_t *inout_total_elems);

/* Batch insert from linked list (transaction + reused statement) */
UTAX_API utax_rc utax_corporate_actions_insert_many_list(utax_db_t *db,
                                                         utax_corporate_actions_node *head,
                                                         size_t *out_inserted);

/* parse -> insert -> free (always frees list) */
UTAX_API utax_rc utax_corporate_actions_insert_many_from_csv_file(utax_db_t *db,
                                                                  const char *path,
                                                                  size_t *out_inserted);

#ifdef __cplusplus
}
#endif
