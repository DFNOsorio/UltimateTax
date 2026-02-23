#include "utax_trades.h"
#include "utax_db_priv.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static utax_rc utax__bind_filters(struct utax_db *h, sqlite3_stmt *st, const utax_trades_filter *f, int *io_idx) {
    int idx = *io_idx;

    if (f && f->has_year) {
        if (sqlite3_bind_int(st, idx++, f->year) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f && f->has_broker) {
        if (sqlite3_bind_text(st, idx++, f->broker, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f && f->has_ticker) {
        if (sqlite3_bind_text(st, idx++, f->ticker, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f && f->has_type) {
        if (sqlite3_bind_text(st, idx++, f->type, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }

    *io_idx = idx;
    return UTAX_OK;
}

static utax_rc utax__build_where_plain(char *sql, size_t sql_sz, const utax_trades_filter *f) {
    size_t len = strlen(sql);

    #define APPEND_LIT(lit) \
      do { \
        size_t n = strlen(lit); \
        if (len + n + 1 > sql_sz) return UTAX_ERR_INVALID_ARG; \
        memcpy(sql + len, lit, n + 1); \
        len += n; \
      } while (0)

    if (f && f->has_year) {
        if (f->year_mode == UTAX_YEAR_UP_TO) APPEND_LIT(" AND trade_year <= ?");
        else                                 APPEND_LIT(" AND trade_year = ?");
    }
    if (f && f->has_broker) APPEND_LIT(" AND broker = ?");
    if (f && f->has_ticker) APPEND_LIT(" AND ticker = ?");
    if (f && f->has_type)   APPEND_LIT(" AND type = ?");

    return UTAX_OK;

    #undef APPEND_LIT
}

utax_rc utax_trades_insert(utax_db_t *db, const utax_trades_row *row, long long *out_id) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;

    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "INSERT INTO trades ("
        " broker, ticker, trade_datetime, type, "
        " quantity, price_per_share, commission, "
        " country, currency, conversion_rate_eur"
        ") VALUES ("
        " COALESCE(NULLIF(?1,''),'IKBR'),"
        " ?2,"
        " ?3,"
        " COALESCE(NULLIF(?4,''),'BUY'),"
        " ?5, ?6, ?7,"
        " COALESCE(NULLIF(?8,''),'US'),"
        " COALESCE(NULLIF(?9,''),'USD'),"
        " ?10"
        ");";

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    /* numeric defaults */
    double commission = (row->commission >= 0.0) ? row->commission : 0.0;
    double conv = (row->conversion_rate_eur > 0.0) ? row->conversion_rate_eur : 1.0;

    (void)utax__bind_text(st, 1, row->broker);
    (void)utax__bind_text(st, 2, row->ticker);
    (void)utax__bind_text(st, 3, row->trade_datetime);
    (void)utax__bind_text(st, 4, row->type);

    sqlite3_bind_double(st, 5, row->quantity);
    sqlite3_bind_double(st, 6, row->price_per_share);
    sqlite3_bind_double(st, 7, commission);

    (void)utax__bind_text(st, 8, row->country);
    (void)utax__bind_text(st, 9, row->currency);
    sqlite3_bind_double(st, 10, conv);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (out_id) *out_id = (long long)sqlite3_last_insert_rowid(h->db);
    return UTAX_OK;
}

utax_rc utax_trades_update_by_id(utax_db_t *db, long long id, const utax_trades_row *row) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;

    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "UPDATE trades SET "
        " broker = COALESCE(NULLIF(?1,''),'IKBR'),"
        " ticker = ?2,"
        " trade_datetime = ?3,"
        " type = COALESCE(NULLIF(?4,''),'BUY'),"
        " quantity = ?5,"
        " price_per_share = ?6,"
        " commission = ?7,"
        " country = COALESCE(NULLIF(?8,''),'US'),"
        " currency = COALESCE(NULLIF(?9,''),'USD'),"
        " conversion_rate_eur = ?10"
        " WHERE id = ?11;";

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    double commission = (row->commission >= 0.0) ? row->commission : 0.0;
    double conv = (row->conversion_rate_eur > 0.0) ? row->conversion_rate_eur : 1.0;

    (void)utax__bind_text(st, 1, row->broker);
    (void)utax__bind_text(st, 2, row->ticker);
    (void)utax__bind_text(st, 3, row->trade_datetime);
    (void)utax__bind_text(st, 4, row->type);

    sqlite3_bind_double(st, 5, row->quantity);
    sqlite3_bind_double(st, 6, row->price_per_share);
    sqlite3_bind_double(st, 7, commission);

    (void)utax__bind_text(st, 8, row->country);
    (void)utax__bind_text(st, 9, row->currency);
    sqlite3_bind_double(st, 10, conv);

    sqlite3_bind_int64(st, 11, (sqlite3_int64)id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "trade id not found");
        return UTAX_ERR_NOT_FOUND;
    }

    return UTAX_OK;
}

utax_rc utax_trades_delete_by_id(utax_db_t *db, long long id) {
    if (!db) return UTAX_ERR_INVALID_ARG;

    struct utax_db *h = (struct utax_db *)db;

    const char *sql = "DELETE FROM trades WHERE id = ?1;";
    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_int64(st, 1, (sqlite3_int64)id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "trade id not found");
        return UTAX_ERR_NOT_FOUND;
    }

    return UTAX_OK;
}

static utax_rc utax__append_pagination(char *sql, size_t sql_sz, const utax_trades_filter *f) {
    if (!f) return UTAX_OK;

    /* SQLite allows LIMIT -1 to mean “no limit” (useful if only offset is set) */
    if (f->has_limit || f->has_offset) {
        size_t len = strlen(sql);
        const char *lit = " LIMIT ? OFFSET ?";
        if (len + strlen(lit) + 1 > sql_sz) return UTAX_ERR_INVALID_ARG;
        UTAX_STRCAT(sql, sql_sz, lit);
    }
    return UTAX_OK;
}

static utax_rc utax__bind_pagination(struct utax_db *h, sqlite3_stmt *st, const utax_trades_filter *f, int *io_idx) {
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

utax_rc utax_trades_count_page(utax_db_t *db, const utax_trades_filter *f, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[768];
    UTAX_STRNCPY(sql, sizeof(sql),
        "SELECT COUNT(*) FROM ("
        " SELECT id FROM trades WHERE 1=1"
    );

    utax_rc rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    /* deterministic order before pagination */
    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY trade_datetime ASC, id ASC")) return UTAX_ERR_INVALID_ARG;

    rc = utax__append_pagination(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " );")) return UTAX_ERR_INVALID_ARG;

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

utax_rc utax_trades_count_total(utax_db_t *db, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;

    struct utax_db *h = (struct utax_db *)db;
    sqlite3_stmt *st = NULL;

    utax_rc rc = utax__prep(h, &st, "SELECT COUNT(*) FROM trades;");
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

utax_rc utax_trades_count_filtered(utax_db_t *db, const utax_trades_filter *f, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;

    struct utax_db *h = (struct utax_db *)db;

    char sql[512];
    UTAX_STRNCPY(sql, sizeof(sql), "SELECT COUNT(*) FROM trades WHERE 1=1");

    utax_rc rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    /* prepare + bind */
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

static void utax__col_text(sqlite3_stmt *st, int col, char *dst, size_t dst_sz) {
    const unsigned char *t = sqlite3_column_text(st, col);
    if (!t) { if (dst_sz) dst[0] = '\0'; return; }
    UTAX_STRNCPY(dst, dst_sz, (const char *)t);
}

utax_rc utax_trades_get_filtered(utax_db_t *db,
                                 const utax_trades_filter *f,
                                 utax_trades_row *out_rows,
                                 size_t out_cap,
                                 size_t *out_count,
                                 size_t *out_required)
{
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    if (!out_rows && out_cap != 0) return UTAX_ERR_INVALID_ARG;

    *out_count = 0;
    if (out_required) *out_required = 0;

    struct utax_db *h = (struct utax_db *)db;

    /* 1) Capacity check against the PAGE size (limit/offset applied).
          Default (no limit/offset) == “read all”. */
    long long needed_ll = 0;
    utax_rc rc = utax_trades_count_page(db, f, &needed_ll);
    if (rc != UTAX_OK) return rc;

    size_t needed = (needed_ll <= 0) ? 0 : (size_t)needed_ll;
    if (out_required) *out_required = needed;

    if (needed > out_cap) {
        utax__set_err_msg(h, "output array too small");
        return UTAX_ERR_NO_SPACE;
    }

    /* 2) Build SELECT with optional filters + deterministic order + optional pagination */
    char sql[1024];
    UTAX_STRNCPY(sql, sizeof(sql),
        "SELECT "
        " id, broker, trade_datetime, trade_year, type, ticker, "
        " quantity, price_per_share, commission, country, currency, conversion_rate_eur "
        "FROM trades "
        "WHERE 1=1"
    );

    rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY trade_datetime ASC, id ASC")) {
        return UTAX_ERR_INVALID_ARG;
    }

    /* Pagination: if either limit or offset is set, bind both (LIMIT ? OFFSET ?) */
    int want_pagination = (f && (f->has_limit || f->has_offset)) ? 1 : 0;
    if (want_pagination) {
        if (!UTAX_STRCAT(sql, sizeof(sql), " LIMIT ? OFFSET ?")) {
            return UTAX_ERR_INVALID_ARG;
        }
    }

    if (!UTAX_STRCAT(sql, sizeof(sql), ";")) {
        return UTAX_ERR_INVALID_ARG;
    }

    sqlite3_stmt *st = NULL;
    rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    /* 3) Bind filters */
    int idx = 1;
    rc = utax__bind_filters(h, st, f, &idx);
    if (rc != UTAX_OK) {
        sqlite3_finalize(st);
        return rc;
    }

    /* 4) Bind pagination (after filters) */
    if (want_pagination) {
        int limit  = (f && f->has_limit)  ? f->limit  : -1; /* -1 means “no limit” */
        int offset = (f && f->has_offset) ? f->offset : 0;

        if (limit < -1 || offset < 0) {
            sqlite3_finalize(st);
            utax__set_err_msg(h, "invalid pagination");
            return UTAX_ERR_INVALID_ARG;
        }

        if (sqlite3_bind_int(st, idx++, limit) != SQLITE_OK) {
            sqlite3_finalize(st);
            return utax__set_err_sqlite(h, SQLITE_ERROR);
        }
        if (sqlite3_bind_int(st, idx++, offset) != SQLITE_OK) {
            sqlite3_finalize(st);
            return utax__set_err_sqlite(h, SQLITE_ERROR);
        }
    }

    /* 5) Read rows into preallocated output */
    size_t i = 0;
    int s = SQLITE_OK;

    while ((s = sqlite3_step(st)) == SQLITE_ROW) {
        if (i >= out_cap) {
            /* Should never happen due to the count_page() capacity check */
            sqlite3_finalize(st);
            utax__set_err_msg(h, "output array too small");
            return UTAX_ERR_NO_SPACE;
        }

        utax_trades_row *r = &out_rows[i++];

        r->id = (long long)sqlite3_column_int64(st, 0);
        utax__col_text(st, 1,  r->broker, sizeof(r->broker));
        utax__col_text(st, 2,  r->trade_datetime, sizeof(r->trade_datetime));
        r->trade_year = sqlite3_column_int(st, 3);
        utax__col_text(st, 4,  r->type, sizeof(r->type));
        utax__col_text(st, 5,  r->ticker, sizeof(r->ticker));

        r->quantity = sqlite3_column_double(st, 6);
        r->price_per_share = sqlite3_column_double(st, 7);
        r->commission = sqlite3_column_double(st, 8);

        utax__col_text(st, 9,  r->country, sizeof(r->country));
        utax__col_text(st, 10, r->currency, sizeof(r->currency));
        r->conversion_rate_eur = sqlite3_column_double(st, 11);
    }

    sqlite3_finalize(st);

    if (s != SQLITE_DONE) {
        return utax__set_err_sqlite(h, s);
    }

    *out_count = i;
    return UTAX_OK;
}
