#include "utax_dividends.h"
#include "utax_db_priv.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static utax_rc utax__build_where_plain(char *sql, size_t sql_sz, const utax_dividends_filter *f) {
    if (!f) return UTAX_OK;

    if (f->has_year) {
        if (f->year_mode == UTAX_YEAR_UP_TO) {
            if (!UTAX_STRCAT(sql, sql_sz, " AND dividend_year <= ?")) return UTAX_ERR_INVALID_ARG;
        } else {
            if (!UTAX_STRCAT(sql, sql_sz, " AND dividend_year = ?")) return UTAX_ERR_INVALID_ARG;
        }
    }

    if (f->has_broker)   { if (!UTAX_STRCAT(sql, sql_sz, " AND broker = ?"))   return UTAX_ERR_INVALID_ARG; }
    if (f->has_ticker)   { if (!UTAX_STRCAT(sql, sql_sz, " AND ticker = ?"))   return UTAX_ERR_INVALID_ARG; }
    if (f->has_country)  { if (!UTAX_STRCAT(sql, sql_sz, " AND country = ?"))  return UTAX_ERR_INVALID_ARG; }
    if (f->has_currency) { if (!UTAX_STRCAT(sql, sql_sz, " AND currency = ?")) return UTAX_ERR_INVALID_ARG; }

    return UTAX_OK;
}

static utax_rc utax__bind_filters(struct utax_db *h, sqlite3_stmt *st, const utax_dividends_filter *f, int *io_idx) {
    int idx = *io_idx;
    if (!f) return UTAX_OK;

    if (f->has_year) {
        if (sqlite3_bind_int(st, idx++, f->year) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f->has_broker) {
        if (sqlite3_bind_text(st, idx++, f->broker, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f->has_ticker) {
        if (sqlite3_bind_text(st, idx++, f->ticker, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f->has_country) {
        if (sqlite3_bind_text(st, idx++, f->country, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f->has_currency) {
        if (sqlite3_bind_text(st, idx++, f->currency, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }

    *io_idx = idx;
    return UTAX_OK;
}

static utax_rc utax__bind_pagination(struct utax_db *h, sqlite3_stmt *st, const utax_dividends_filter *f, int *io_idx) {
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

static void utax__col_text(sqlite3_stmt *st, int col, char *dst, size_t dst_sz) {
    const unsigned char *t = sqlite3_column_text(st, col);
    if (!t) { if (dst_sz) dst[0] = '\0'; return; }
    UTAX_STRNCPY(dst, dst_sz, (const char *)t);
}

/* ───────────────────────────── CRUD ───────────────────────────── */

utax_rc utax_dividends_insert(utax_db_t *db, const utax_dividends_row *row, long long *out_id) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "INSERT INTO dividends (broker, ticker, country, dividend_dt, per_share, total_amount, tax, currency, conversion_rate_eur) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);";

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_text(st, 1, row->broker, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, row->ticker, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, row->country, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, row->dividend_dt, -1, SQLITE_TRANSIENT);

    sqlite3_bind_double(st, 5, row->per_share);
    sqlite3_bind_double(st, 6, row->total_amount);
    sqlite3_bind_double(st, 7, row->tax);

    sqlite3_bind_text(st, 8, row->currency, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 9, row->conversion_rate_eur);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (out_id) *out_id = (long long)sqlite3_last_insert_rowid(h->db);
    return UTAX_OK;
}

utax_rc utax_dividends_insert_many(utax_db_t *db,
                                   utax_dividends_row *rows,
                                   size_t n,
                                   size_t *out_inserted)
{
    if (!db || (!rows && n != 0)) return UTAX_ERR_INVALID_ARG;
    if (out_inserted) *out_inserted = 0;
    if (n == 0) return UTAX_OK;

    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "INSERT INTO dividends ("
        " broker, ticker, country, dividend_dt, "
        " per_share, total_amount, tax, "
        " currency, conversion_rate_eur"
        ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);";

    /* Begin transaction (DEFERRED) */
    {
        int rc0 = sqlite3_exec(h->db, "BEGIN;", NULL, NULL, NULL);
        if (rc0 != SQLITE_OK) return utax__set_err_sqlite(h, rc0);
    }

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) {
        (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
        return rc;
    }

    size_t i = 0;
    for (; i < n; ++i) {
        utax_dividends_row *r = &rows[i];

        sqlite3_clear_bindings(st);
        sqlite3_reset(st);

        (void)utax__bind_text(st, 1, r->broker);
        (void)utax__bind_text(st, 2, r->ticker);
        (void)utax__bind_text(st, 3, r->country);
        (void)utax__bind_text(st, 4, r->dividend_dt);

        sqlite3_bind_double(st, 5, r->per_share);
        sqlite3_bind_double(st, 6, r->total_amount);
        sqlite3_bind_double(st, 7, r->tax);

        (void)utax__bind_text(st, 8, r->currency);
        sqlite3_bind_double(st, 9, r->conversion_rate_eur);

        int s = sqlite3_step(st);
        if (s != SQLITE_DONE) {
            sqlite3_finalize(st);
            (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
            if (out_inserted) *out_inserted = i;
            return utax__set_err_sqlite(h, s);
        }

        r->dividend_id = (long long)sqlite3_last_insert_rowid(h->db);
    }

    sqlite3_finalize(st);

    {
        int rc1 = sqlite3_exec(h->db, "COMMIT;", NULL, NULL, NULL);
        if (rc1 != SQLITE_OK) {
            (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
            if (out_inserted) *out_inserted = i;
            return utax__set_err_sqlite(h, rc1);
        }
    }

    if (out_inserted) *out_inserted = n;
    return UTAX_OK;
}

utax_rc utax_dividends_update_by_id(utax_db_t *db, long long id, const utax_dividends_row *row) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "UPDATE dividends SET "
        " broker=?1, ticker=?2, country=?3, dividend_dt=?4, "
        " per_share=?5, total_amount=?6, tax=?7, "
        " currency=?8, conversion_rate_eur=?9 "
        "WHERE dividend_id=?10;";

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_text(st, 1, row->broker, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, row->ticker, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, row->country, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, row->dividend_dt, -1, SQLITE_TRANSIENT);

    sqlite3_bind_double(st, 5, row->per_share);
    sqlite3_bind_double(st, 6, row->total_amount);
    sqlite3_bind_double(st, 7, row->tax);

    sqlite3_bind_text(st, 8, row->currency, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(st, 9, row->conversion_rate_eur);

    sqlite3_bind_int64(st, 10, (sqlite3_int64)id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "dividend id not found");
        return UTAX_ERR_NOT_FOUND;
    }
    return UTAX_OK;
}

utax_rc utax_dividends_delete_by_id(utax_db_t *db, long long id) {
    if (!db) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, "DELETE FROM dividends WHERE dividend_id=?1;");
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_int64(st, 1, (sqlite3_int64)id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "dividend id not found");
        return UTAX_ERR_NOT_FOUND;
    }
    return UTAX_OK;
}

/* ───────────────────────────── Counts ───────────────────────────── */

utax_rc utax_dividends_count_total(utax_db_t *db, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, "SELECT COUNT(*) FROM dividends;");
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

utax_rc utax_dividends_count_filtered(utax_db_t *db, const utax_dividends_filter *f, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[768];
    UTAX_STRNCPY(sql, sizeof(sql), "SELECT COUNT(*) FROM dividends WHERE 1=1");

    utax_rc rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    sqlite3_stmt *st = NULL;
    rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    int idx = 1;
    rc = utax__bind_filters(h, st, f, &idx);
    if (rc != UTAX_OK) { sqlite3_finalize(st); return rc; }

    int s = sqlite3_step(st);
    if (s == SQLITE_ROW) {
        *out_count = (long long)sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
        return UTAX_OK;
    }
    sqlite3_finalize(st);
    return utax__set_err_sqlite(h, s);
}

utax_rc utax_dividends_count_page(utax_db_t *db, const utax_dividends_filter *f, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[1024];
    UTAX_STRNCPY(sql, sizeof(sql), "SELECT COUNT(*) FROM (SELECT dividend_id FROM dividends WHERE 1=1");

    utax_rc rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY dividend_dt ASC, dividend_id ASC")) return UTAX_ERR_INVALID_ARG;

    int want_pagination = (f && (f->has_limit || f->has_offset)) ? 1 : 0;
    if (want_pagination) {
        if (!UTAX_STRCAT(sql, sizeof(sql), " LIMIT ? OFFSET ?")) return UTAX_ERR_INVALID_ARG;
    }

    if (!UTAX_STRCAT(sql, sizeof(sql), ");")) return UTAX_ERR_INVALID_ARG;

    sqlite3_stmt *st = NULL;
    rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    int idx = 1;
    rc = utax__bind_filters(h, st, f, &idx);
    if (rc != UTAX_OK) { sqlite3_finalize(st); return rc; }

    rc = utax__bind_pagination(h, st, f, &idx);
    if (rc != UTAX_OK) { sqlite3_finalize(st); return rc; }

    int s = sqlite3_step(st);
    if (s == SQLITE_ROW) {
        *out_count = (long long)sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
        return UTAX_OK;
    }
    sqlite3_finalize(st);
    return utax__set_err_sqlite(h, s);
}

/* ───────────────────────────── Query rows ───────────────────────────── */

utax_rc utax_dividends_get_filtered(utax_db_t *db,
                                    const utax_dividends_filter *f,
                                    utax_dividends_row *out_rows,
                                    size_t out_cap,
                                    size_t *out_count,
                                    size_t *out_required)
{
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    if (!out_rows && out_cap != 0) return UTAX_ERR_INVALID_ARG;

    *out_count = 0;
    if (out_required) *out_required = 0;

    struct utax_db *h = (struct utax_db *)db;

    /* capacity check vs page size */
    long long needed_ll = 0;
    utax_rc rc = utax_dividends_count_page(db, f, &needed_ll);
    if (rc != UTAX_OK) return rc;

    size_t needed = (needed_ll <= 0) ? 0 : (size_t)needed_ll;
    if (out_required) *out_required = needed;

    if (needed > out_cap) {
        utax__set_err_msg(h, "output array too small");
        return UTAX_ERR_NO_SPACE;
    }

    char sql[1024];
    UTAX_STRNCPY(sql, sizeof(sql),
        "SELECT "
        " dividend_id, broker, ticker, country, dividend_dt, dividend_year, "
        " per_share, total_amount, tax, currency, conversion_rate_eur "
        "FROM dividends WHERE 1=1"
    );

    rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY dividend_dt ASC, dividend_id ASC")) return UTAX_ERR_INVALID_ARG;

    int want_pagination = (f && (f->has_limit || f->has_offset)) ? 1 : 0;
    if (want_pagination) {
        if (!UTAX_STRCAT(sql, sizeof(sql), " LIMIT ? OFFSET ?")) return UTAX_ERR_INVALID_ARG;
    }

    if (!UTAX_STRCAT(sql, sizeof(sql), ";")) return UTAX_ERR_INVALID_ARG;

    sqlite3_stmt *st = NULL;
    rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    int idx = 1;
    rc = utax__bind_filters(h, st, f, &idx);
    if (rc != UTAX_OK) { sqlite3_finalize(st); return rc; }

    rc = utax__bind_pagination(h, st, f, &idx);
    if (rc != UTAX_OK) { sqlite3_finalize(st); return rc; }

    size_t i = 0;
    int s = SQLITE_OK;
    while ((s = sqlite3_step(st)) == SQLITE_ROW) {
        utax_dividends_row *r = &out_rows[i++];

        r->dividend_id = (long long)sqlite3_column_int64(st, 0);
        utax__col_text(st, 1, r->broker, sizeof(r->broker));
        utax__col_text(st, 2, r->ticker, sizeof(r->ticker));
        utax__col_text(st, 3, r->country, sizeof(r->country));
        utax__col_text(st, 4, r->dividend_dt, sizeof(r->dividend_dt));
        r->dividend_year = sqlite3_column_int(st, 5);

        r->per_share = sqlite3_column_double(st, 6);
        r->total_amount = sqlite3_column_double(st, 7);
        r->tax = sqlite3_column_double(st, 8);

        utax__col_text(st, 9, r->currency, sizeof(r->currency));
        r->conversion_rate_eur = sqlite3_column_double(st, 10);

        if (i >= out_cap) break; /* defensive */
    }

    sqlite3_finalize(st);

    if (s != SQLITE_DONE && s != SQLITE_ROW) return utax__set_err_sqlite(h, s);

    *out_count = i;
    return UTAX_OK;
}
