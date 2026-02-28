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
/**
 * @brief Inserts one snapshot/action-applied row.
 * @param db Open database handle.
 * @param row Row payload to insert.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_insert(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_row *row
);

/**
 * @brief Inserts multiple snapshot/action-applied rows.
 * @param db Open database handle.
 * @param rows Rows to insert.
 * @param n Number of elements in @p rows.
 * @param out_inserted Optional output for inserted row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_insert_many(
    utax_db_t *db,
    utax_fifo_snapshot_action_applied_row *rows,
    size_t n,
    size_t *out_inserted
);

/**
 * @brief Updates a row identified by composite keys.
 * @param db Open database handle.
 * @param lot_id Existing lot id key.
 * @param action_id Existing action id key.
 * @param row Replacement row data.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_update_by_keys(
    utax_db_t *db,
    long long lot_id,
    long long action_id,
    const utax_fifo_snapshot_action_applied_row *row
);

/**
 * @brief Deletes a row identified by composite keys.
 * @param db Open database handle.
 * @param lot_id Existing lot id key.
 * @param action_id Existing action id key.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_delete_by_keys(
    utax_db_t *db,
    long long lot_id,
    long long action_id
);

/* Counts */
/**
 * @brief Counts all snapshot/action-applied rows.
 * @param db Open database handle.
 * @param out_count Output total row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_count_total(
    utax_db_t *db,
    long long *out_count
);

/**
 * @brief Counts rows matching a filter.
 * @param db Open database handle.
 * @param f Optional filter criteria.
 * @param out_count Output matching row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_count_filtered(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_filter *f,
    long long *out_count
);

/**
 * @brief Counts rows returned by the current page settings in a filter.
 * @param db Open database handle.
 * @param f Optional filter criteria including pagination.
 * @param out_count Output paged row count.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_fifo_snapshot_action_applied_count_page(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_filter *f,
    long long *out_count
);

/* Query rows (paged) */
/**
 * @brief Fetches paged rows matching a filter.
 * @param db Open database handle.
 * @param f Optional filter criteria including pagination.
 * @param out_rows Output buffer for fetched rows.
 * @param out_cap Capacity of @p out_rows in elements.
 * @param out_count Output number of written rows.
 * @param out_required Optional output required capacity when @p out_cap is insufficient.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
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
