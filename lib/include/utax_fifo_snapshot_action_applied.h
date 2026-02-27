#pragma once
/**
 * @file utax_fifo_snapshot_action_applied.h
 * @brief CRUD and query API for FIFO snapshot/action-applied link rows.
 */
#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Filter options for querying FIFO snapshot/action-applied rows. */
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
/** @brief Insert one snapshot/action-applied row. */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_insert(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_row *row
);

/** @brief Insert multiple snapshot/action-applied rows. */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_insert_many(
    utax_db_t *db,
    utax_fifo_snapshot_action_applied_row *rows,
    size_t n,
    size_t *out_inserted
);

/** @brief Update a row identified by composite keys. */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_update_by_keys(
    utax_db_t *db,
    long long lot_id,
    long long action_id,
    const utax_fifo_snapshot_action_applied_row *row
);

/** @brief Delete a row identified by composite keys. */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_delete_by_keys(
    utax_db_t *db,
    long long lot_id,
    long long action_id
);

/* Counts */
/** @brief Count all snapshot/action-applied rows. */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_count_total(
    utax_db_t *db,
    long long *out_count
);

/** @brief Count rows matching a filter. */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_count_filtered(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_filter *f,
    long long *out_count
);

/** @brief Count rows returned by the current page settings in a filter. */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_count_page(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_filter *f,
    long long *out_count
);

/* Query rows (paged) */
/** @brief Fetch paged rows matching a filter. */
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
