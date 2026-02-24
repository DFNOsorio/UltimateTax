#pragma once
#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct utax_fifo_snapshot_action_applied_filter {
    int has_lot_id;
    long long lot_id;

    int has_action_id;
    long long action_id;

    int has_limit;
    int limit;

    int has_offset;
    int offset;
} utax_fifo_snapshot_action_applied_filter;

/* CRUD */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_insert(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_row *row
);

UTAX_API utax_rc utax_fifo_snapshot_action_applied_insert_many(
    utax_db_t *db,
    utax_fifo_snapshot_action_applied_row *rows,
    size_t n,
    size_t *out_inserted
);

UTAX_API utax_rc utax_fifo_snapshot_action_applied_update_by_keys(
    utax_db_t *db,
    long long lot_id,
    long long action_id,
    const utax_fifo_snapshot_action_applied_row *row
);

UTAX_API utax_rc utax_fifo_snapshot_action_applied_delete_by_keys(
    utax_db_t *db,
    long long lot_id,
    long long action_id
);

/* Counts */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_count_total(
    utax_db_t *db,
    long long *out_count
);

UTAX_API utax_rc utax_fifo_snapshot_action_applied_count_filtered(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_filter *f,
    long long *out_count
);

UTAX_API utax_rc utax_fifo_snapshot_action_applied_count_page(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_filter *f,
    long long *out_count
);

/* Query rows (paged) */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_get_filtered(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_filter *f,
    utax_fifo_snapshot_action_applied_row *out_rows,
    size_t out_cap,
    size_t *out_count,
    size_t *out_required
);

#ifdef __cplusplus
}
#endif
