#include "utax_trades.h"
#include "utax_db_priv.h"

#include <sqlite3.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
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

utax_rc utax_trades_insert_many(utax_db_t *db,
                                utax_trades_row *rows,
                                size_t n,
                                size_t *out_inserted)
{
    if (!db || (!rows && n != 0)) return UTAX_ERR_INVALID_ARG;
    if (out_inserted) *out_inserted = 0;
    if (n == 0) return UTAX_OK;

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
        utax_trades_row *r = &rows[i];

        sqlite3_clear_bindings(st);
        sqlite3_reset(st);

        /* numeric defaults consistent with single insert */
        double commission = (r->commission >= 0.0) ? r->commission : 0.0;
        double conv = (r->conversion_rate_eur > 0.0) ? r->conversion_rate_eur : 1.0;

        (void)utax__bind_text(st, 1, r->broker);
        (void)utax__bind_text(st, 2, r->ticker);
        (void)utax__bind_text(st, 3, r->trade_datetime);
        (void)utax__bind_text(st, 4, r->type);

        sqlite3_bind_double(st, 5, r->quantity);
        sqlite3_bind_double(st, 6, r->price_per_share);
        sqlite3_bind_double(st, 7, commission);

        (void)utax__bind_text(st, 8, r->country);
        (void)utax__bind_text(st, 9, r->currency);
        sqlite3_bind_double(st, 10, conv);

        int s = sqlite3_step(st);
        if (s != SQLITE_DONE) {
            sqlite3_finalize(st);
            (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
            if (out_inserted) *out_inserted = i;
            return utax__set_err_sqlite(h, s);
        }

        r->id = (long long)sqlite3_last_insert_rowid(h->db);
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

/* ---------- helpers ---------- */

static char *utax__trim_ws(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';
    return s;
}

/* Simple CSV split (no quoted commas). */
static int utax__split_csv_simple(char *line, char **out_fields, int max_fields) {
    int n = 0;
    char *p = line;
    while (*p && n < max_fields) {
        out_fields[n++] = p;
        char *comma = strchr(p, ',');
        if (!comma) break;
        *comma = '\0';
        p = comma + 1;
    }
    return n;
}

static utax_rc utax__copy_field(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return UTAX_ERR_INVALID_ARG;
    if (!src) src = "";

    size_t sl = strlen(src);
    if (sl >= dst_sz) {
        UTAX_STRNCPY(dst, dst_sz, src);
        return UTAX_ERR_TRUNCATED;
    }

    UTAX_STRNCPY(dst, dst_sz, src);
    return UTAX_OK;
}

static utax_rc utax__parse_double_strict(const char *s, double *out) {
    if (!s || !out) return UTAX_ERR_INVALID_ARG;

    errno = 0;
    char *end = NULL;
    double v = strtod(s, &end);

    if (end == s) return UTAX_ERR_BAD_FIELD;
    while (end && *end && isspace((unsigned char)*end)) end++;
    if (end && *end) return UTAX_ERR_BAD_FIELD;

    if (errno == ERANGE) return UTAX_ERR_OVERFLOW;

    *out = v;
    return UTAX_OK;
}

static utax_rc utax__normalize_trade_datetime(const char *date10, const char *time5, char out_dt[UTAX_DT_MAX]) {
    /* DATE: "YYYY-MM-DD" (10 chars), TIME: "HH:MM" (5 chars) => "YYYY-MM-DD HH:MM" (16 chars) */
    if (!date10 || !time5) return UTAX_ERR_BAD_FIELD;
    if (strlen(date10) != 10 || date10[4] != '-' || date10[7] != '-') return UTAX_ERR_BAD_FIELD;
    if (strlen(time5) != 5 || time5[2] != ':') return UTAX_ERR_BAD_FIELD;

    int n = snprintf(out_dt, UTAX_DT_MAX, "%s %s", date10, time5);
    if (n != 16) return UTAX_ERR_BAD_FIELD;
    return UTAX_OK;
}

static utax_rc utax__validate_trades_header(char *hdr_line) {
    static const char *k_expected[11] = {
        "DATE","TIME","BROKER","TYPE","TICKER","QTD","PER_SHARE","COMMISSION","COUNTRY","CURRENCY","CONVERSION_RATE_1EUR"
    };

    char *fields[16] = {0};
    int nf = utax__split_csv_simple(hdr_line, fields, 16);
    if (nf != 11) return UTAX_ERR_BAD_HEADER;

    for (int i = 0; i < nf; ++i) fields[i] = utax__trim_ws(fields[i]);
    for (int i = 0; i < 11; ++i) {
        if (strcmp(fields[i], k_expected[i]) != 0) return UTAX_ERR_BAD_HEADER;
    }
    return UTAX_OK;
}

/* ---------- parse/free rows ---------- */

utax_rc utax_trades_parse_csv_file(
    const char *path,
    utax_trades_row **inout_rows,
    size_t *inout_total_elems
) {
    if (!path || !inout_rows || !inout_total_elems) return UTAX_ERR_INVALID_ARG;

    FILE *f = NULL;
    if (!UTAX_FOPEN(f, path, "rb")) return UTAX_ERR_IO_OPEN;

    utax_trades_row *parsed_rows = NULL;
    size_t parsed_count = 0;
    size_t parsed_cap = 0;

    char line[4096];

    /* header */
    if (!fgets(line, sizeof(line), f)) {
        if (ferror(f)) { fclose(f); return UTAX_ERR_IO_READ; }
        fclose(f);
        return UTAX_OK; /* empty file */
    }

    char *nl = strpbrk(line, "\r\n");
    if (nl) *nl = '\0';

    char *hdr = utax__trim_ws(line);
    if (!strchr(hdr, ',')) { fclose(f); return UTAX_ERR_UNSUPPORTED; }

    utax_rc hrc = utax__validate_trades_header(hdr);
    if (hrc != UTAX_OK) { fclose(f); return hrc; }

    /* rows */
    while (fgets(line, sizeof(line), f)) {
        char *nl2 = strpbrk(line, "\r\n");
        if (nl2) *nl2 = '\0';

        char *s = utax__trim_ws(line);
        if (!*s) continue;

        char *fields[16] = {0};
        int nf = utax__split_csv_simple(s, fields, 16);
        if (nf != 11) {
            fclose(f);
            return UTAX_ERR_PARSE;
        }

        for (int i = 0; i < nf; ++i) fields[i] = utax__trim_ws(fields[i]);

        if (parsed_count == parsed_cap) {
            size_t new_cap = (parsed_cap == 0) ? 8 : (parsed_cap * 2);
            utax_trades_row *grown = (utax_trades_row *)realloc(parsed_rows, new_cap * sizeof(*grown));
            if (!grown) {
                fclose(f);
                free(parsed_rows);
                return UTAX_ERR_NOMEM;
            }
            parsed_rows = grown;
            parsed_cap = new_cap;
        }

        utax_trades_row *r = &parsed_rows[parsed_count];
        memset(r, 0, sizeof(*r));

        /* DATE+TIME -> trade_datetime */
        utax_rc rcdt = utax__normalize_trade_datetime(fields[0], fields[1], r->trade_datetime);
        if (rcdt != UTAX_OK) {
            fclose(f);
            free(parsed_rows);
            return rcdt;
        }

        /* strings */
        utax_rc rc1 = utax__copy_field(r->broker, sizeof(r->broker), fields[2]);
        utax_rc rc2 = utax__copy_field(r->type, sizeof(r->type), fields[3]);
        utax_rc rc3 = utax__copy_field(r->ticker, sizeof(r->ticker), fields[4]);
        utax_rc rc4 = utax__copy_field(r->country, sizeof(r->country), fields[8]);
        utax_rc rc5 = utax__copy_field(r->currency, sizeof(r->currency), fields[9]);

        if (rc1 == UTAX_ERR_TRUNCATED || rc2 == UTAX_ERR_TRUNCATED || rc3 == UTAX_ERR_TRUNCATED ||
            rc4 == UTAX_ERR_TRUNCATED || rc5 == UTAX_ERR_TRUNCATED) {
            fclose(f);
            free(parsed_rows);
            return UTAX_ERR_TRUNCATED;
        }
        if (rc1 != UTAX_OK || rc2 != UTAX_OK || rc3 != UTAX_OK || rc4 != UTAX_OK || rc5 != UTAX_OK) {
            fclose(f);
            free(parsed_rows);
            return UTAX_ERR_PARSE;
        }

        /* validate TYPE */
        if (!(strcmp(r->type, "BUY") == 0 || strcmp(r->type, "SELL") == 0)) {
            fclose(f);
            free(parsed_rows);
            return UTAX_ERR_BAD_FIELD;
        }

        /* numeric: QTD, PER_SHARE, COMMISSION, CONVERSION_RATE_1EUR */
        utax_rc rq = utax__parse_double_strict(fields[5], &r->quantity);
        utax_rc rp = utax__parse_double_strict(fields[6], &r->price_per_share);
        utax_rc rc = utax__parse_double_strict(fields[7], &r->commission);
        utax_rc rr = utax__parse_double_strict(fields[10], &r->conversion_rate_eur);

        if (rq == UTAX_ERR_OVERFLOW || rp == UTAX_ERR_OVERFLOW || rc == UTAX_ERR_OVERFLOW || rr == UTAX_ERR_OVERFLOW) {
            fclose(f);
            free(parsed_rows);
            return UTAX_ERR_OVERFLOW;
        }
        if (rq != UTAX_OK || rp != UTAX_OK || rc != UTAX_OK || rr != UTAX_OK) {
            fclose(f);
            free(parsed_rows);
            return UTAX_ERR_BAD_FIELD;
        }

        /* generated in DB */
        r->trade_year = 0;
        r->id = 0;
        parsed_count++;
    }

    if (ferror(f)) {
        fclose(f);
        free(parsed_rows);
        return UTAX_ERR_IO_READ;
    }

    fclose(f);

    if (parsed_count > 0) {
        size_t base_count = *inout_total_elems;
        utax_trades_row *base_rows = *inout_rows;
        size_t total = base_count + parsed_count;

        utax_trades_row *grown = (utax_trades_row *)realloc(base_rows, total * sizeof(*grown));
        if (!grown) {
            free(parsed_rows);
            return UTAX_ERR_NOMEM;
        }

        memcpy(grown + base_count, parsed_rows, parsed_count * sizeof(*parsed_rows));
        *inout_rows = grown;
        *inout_total_elems = total;
    }

    free(parsed_rows);

    return UTAX_OK;
}

void utax_trades_free_rows(utax_trades_row **inout_rows, size_t *inout_total_elems) {
    if (!inout_rows || !inout_total_elems) return;
    free(*inout_rows);
    *inout_rows = NULL;
    *inout_total_elems = 0;
}

/* ---------- batch insert rows + file ---------- */

utax_rc utax_trades_insert_many_array(utax_db_t *db, utax_trades_row *rows, size_t n, size_t *out_inserted) {
    return utax_trades_insert_many(db, rows, n, out_inserted);
}

utax_rc utax_trades_insert_many_from_csv_file(utax_db_t *db, const char *path, size_t *out_inserted) {
    if (!db || !path) return UTAX_ERR_INVALID_ARG;
    if (out_inserted) *out_inserted = 0;

    utax_trades_row *rows = NULL;
    size_t total = 0;

    utax_rc rc = utax_trades_parse_csv_file(path, &rows, &total);
    if (rc != UTAX_OK) {
        utax_trades_free_rows(&rows, &total);
        return rc;
    }

    size_t inserted = 0;
    rc = utax_trades_insert_many_array(db, rows, total, &inserted);

    utax_trades_free_rows(&rows, &total);

    if (out_inserted) *out_inserted = inserted;
    return rc;
}
