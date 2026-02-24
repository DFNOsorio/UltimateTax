#include "utax_fifo_snapshot_action_applied.h"
#include "utax_db_priv.h"

#include <sqlite3.h>
#include <string.h>

static utax_rc utax__build_where_plain(char *sql, size_t sql_sz, const utax_fifo_snapshot_action_applied_filter *f) {
    if (!f) return UTAX_OK;

    if (f->has_lot_id) {
        if (!UTAX_STRCAT(sql, sql_sz, " AND lot_id = ?")) return UTAX_ERR_INVALID_ARG;
    }
    if (f->has_action_id) {
        if (!UTAX_STRCAT(sql, sql_sz, " AND action_id = ?")) return UTAX_ERR_INVALID_ARG;
    }

    return UTAX_OK;
}

static utax_rc utax__bind_filters(struct utax_db *h, sqlite3_stmt *st, const utax_fifo_snapshot_action_applied_filter *f, int *io_idx) {
    int idx = *io_idx;
    if (!f) return UTAX_OK;

    if (f->has_lot_id) {
        if (sqlite3_bind_int64(st, idx++, (sqlite3_int64)f->lot_id) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f->has_action_id) {
        if (sqlite3_bind_int64(st, idx++, (sqlite3_int64)f->action_id) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }

    *io_idx = idx;
    return UTAX_OK;
}

static utax_rc utax__bind_pagination(struct utax_db *h, sqlite3_stmt *st, const utax_fifo_snapshot_action_applied_filter *f, int *io_idx) {
    if (!f) return UTAX_OK;

    if (f->has_limit || f->has_offset) {
        int limit = f->has_limit ? f->limit : -1;
        int offset = f->has_offset ? f->offset : 0;

        if (limit < -1 || offset < 0) {
            utax__set_err_msg(h, "invalid pagination");
            return UTAX_ERR_INVALID_ARG;
        }

        if (sqlite3_bind_int(st, (*io_idx)++, limit) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
        if (sqlite3_bind_int(st, (*io_idx)++, offset) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }

    return UTAX_OK;
}

utax_rc utax_fifo_snapshot_action_applied_insert(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_row *row
) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st,
        "INSERT INTO fifo_snapshot_action_applied (lot_id, action_id) VALUES (?1, ?2);"
    );
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_int64(st, 1, (sqlite3_int64)row->lot_id);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)row->action_id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);
    return UTAX_OK;
}

utax_rc utax_fifo_snapshot_action_applied_insert_many(
    utax_db_t *db,
    utax_fifo_snapshot_action_applied_row *rows,
    size_t n,
    size_t *out_inserted
) {
    if (!db || (!rows && n != 0)) return UTAX_ERR_INVALID_ARG;
    if (out_inserted) *out_inserted = 0;
    if (n == 0) return UTAX_OK;

    struct utax_db *h = (struct utax_db *)db;

    int rc0 = sqlite3_exec(h->db, "BEGIN;", NULL, NULL, NULL);
    if (rc0 != SQLITE_OK) return utax__set_err_sqlite(h, rc0);

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st,
        "INSERT INTO fifo_snapshot_action_applied (lot_id, action_id) VALUES (?1, ?2);"
    );
    if (rc != UTAX_OK) {
        (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
        return rc;
    }

    size_t i = 0;
    for (; i < n; ++i) {
        sqlite3_clear_bindings(st);
        sqlite3_reset(st);

        sqlite3_bind_int64(st, 1, (sqlite3_int64)rows[i].lot_id);
        sqlite3_bind_int64(st, 2, (sqlite3_int64)rows[i].action_id);

        int s = sqlite3_step(st);
        if (s != SQLITE_DONE) {
            sqlite3_finalize(st);
            (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
            if (out_inserted) *out_inserted = i;
            return utax__set_err_sqlite(h, s);
        }
    }

    sqlite3_finalize(st);

    int rc1 = sqlite3_exec(h->db, "COMMIT;", NULL, NULL, NULL);
    if (rc1 != SQLITE_OK) {
        (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
        if (out_inserted) *out_inserted = i;
        return utax__set_err_sqlite(h, rc1);
    }

    if (out_inserted) *out_inserted = n;
    return UTAX_OK;
}

utax_rc utax_fifo_snapshot_action_applied_update_by_keys(
    utax_db_t *db,
    long long lot_id,
    long long action_id,
    const utax_fifo_snapshot_action_applied_row *row
) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st,
        "UPDATE fifo_snapshot_action_applied SET lot_id=?1, action_id=?2 WHERE lot_id=?3 AND action_id=?4;"
    );
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_int64(st, 1, (sqlite3_int64)row->lot_id);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)row->action_id);
    sqlite3_bind_int64(st, 3, (sqlite3_int64)lot_id);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)action_id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "fifo_snapshot_action_applied row not found");
        return UTAX_ERR_NOT_FOUND;
    }

    return UTAX_OK;
}

utax_rc utax_fifo_snapshot_action_applied_delete_by_keys(
    utax_db_t *db,
    long long lot_id,
    long long action_id
) {
    if (!db) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st,
        "DELETE FROM fifo_snapshot_action_applied WHERE lot_id=?1 AND action_id=?2;"
    );
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_int64(st, 1, (sqlite3_int64)lot_id);
    sqlite3_bind_int64(st, 2, (sqlite3_int64)action_id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "fifo_snapshot_action_applied row not found");
        return UTAX_ERR_NOT_FOUND;
    }

    return UTAX_OK;
}

utax_rc utax_fifo_snapshot_action_applied_count_total(
    utax_db_t *db,
    long long *out_count
) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, "SELECT COUNT(*) FROM fifo_snapshot_action_applied;");
    if (rc != UTAX_OK) return rc;

    int s = sqlite3_step(st);
    if (s == SQLITE_ROW) {
        *out_count = (long long)sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
        return UTAX_OK;
    }

    sqlite3_finalize(st);
    return utax__set_err_sqlite(h, s);
}

utax_rc utax_fifo_snapshot_action_applied_count_filtered(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_filter *f,
    long long *out_count
) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[768];
    UTAX_STRNCPY(sql, sizeof(sql), "SELECT COUNT(*) FROM fifo_snapshot_action_applied WHERE 1=1");

    utax_rc rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    sqlite3_stmt *st = NULL;
    rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    int idx = 1;
    rc = utax__bind_filters(h, st, f, &idx);
    if (rc != UTAX_OK) {
        sqlite3_finalize(st);
        return rc;
    }

    int s = sqlite3_step(st);
    if (s == SQLITE_ROW) {
        *out_count = (long long)sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
        return UTAX_OK;
    }

    sqlite3_finalize(st);
    return utax__set_err_sqlite(h, s);
}

utax_rc utax_fifo_snapshot_action_applied_count_page(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_filter *f,
    long long *out_count
) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[1024];
    UTAX_STRNCPY(sql, sizeof(sql),
        "SELECT COUNT(*) FROM ("
        " SELECT lot_id, action_id FROM fifo_snapshot_action_applied WHERE 1=1"
    );

    utax_rc rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY lot_id ASC, action_id ASC")) return UTAX_ERR_INVALID_ARG;

    if (f && (f->has_limit || f->has_offset)) {
        if (!UTAX_STRCAT(sql, sizeof(sql), " LIMIT ? OFFSET ?")) return UTAX_ERR_INVALID_ARG;
    }

    if (!UTAX_STRCAT(sql, sizeof(sql), " );")) return UTAX_ERR_INVALID_ARG;

    sqlite3_stmt *st = NULL;
    rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    int idx = 1;
    rc = utax__bind_filters(h, st, f, &idx);
    if (rc != UTAX_OK) {
        sqlite3_finalize(st);
        return rc;
    }

    rc = utax__bind_pagination(h, st, f, &idx);
    if (rc != UTAX_OK) {
        sqlite3_finalize(st);
        return rc;
    }

    int s = sqlite3_step(st);
    if (s == SQLITE_ROW) {
        *out_count = (long long)sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
        return UTAX_OK;
    }

    sqlite3_finalize(st);
    return utax__set_err_sqlite(h, s);
}

utax_rc utax_fifo_snapshot_action_applied_get_filtered(
    utax_db_t *db,
    const utax_fifo_snapshot_action_applied_filter *f,
    utax_fifo_snapshot_action_applied_row *out_rows,
    size_t out_cap,
    size_t *out_count,
    size_t *out_required
) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    if (!out_rows && out_cap != 0) return UTAX_ERR_INVALID_ARG;

    *out_count = 0;
    if (out_required) *out_required = 0;

    struct utax_db *h = (struct utax_db *)db;

    long long needed_ll = 0;
    utax_rc rc = utax_fifo_snapshot_action_applied_count_page(db, f, &needed_ll);
    if (rc != UTAX_OK) return rc;

    size_t needed = (needed_ll <= 0) ? 0 : (size_t)needed_ll;
    if (out_required) *out_required = needed;

    if (needed > out_cap) {
        utax__set_err_msg(h, "output array too small");
        return UTAX_ERR_NO_SPACE;
    }

    char sql[1024];
    UTAX_STRNCPY(sql, sizeof(sql),
        "SELECT lot_id, action_id FROM fifo_snapshot_action_applied WHERE 1=1"
    );

    rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY lot_id ASC, action_id ASC")) return UTAX_ERR_INVALID_ARG;

    if (f && (f->has_limit || f->has_offset)) {
        if (!UTAX_STRCAT(sql, sizeof(sql), " LIMIT ? OFFSET ?")) return UTAX_ERR_INVALID_ARG;
    }

    if (!UTAX_STRCAT(sql, sizeof(sql), ";")) return UTAX_ERR_INVALID_ARG;

    sqlite3_stmt *st = NULL;
    rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    int idx = 1;
    rc = utax__bind_filters(h, st, f, &idx);
    if (rc != UTAX_OK) {
        sqlite3_finalize(st);
        return rc;
    }

    rc = utax__bind_pagination(h, st, f, &idx);
    if (rc != UTAX_OK) {
        sqlite3_finalize(st);
        return rc;
    }

    size_t i = 0;
    int s = SQLITE_OK;

    while ((s = sqlite3_step(st)) == SQLITE_ROW) {
        out_rows[i].lot_id = (long long)sqlite3_column_int64(st, 0);
        out_rows[i].action_id = (long long)sqlite3_column_int64(st, 1);
        i++;
    }

    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    *out_count = i;
    return UTAX_OK;
}
