#include "utax_options.h"
#include "utax_db_priv.h"

#include <sqlite3.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void utax__col_text(sqlite3_stmt *st, int col, char *dst, size_t dst_sz) {
    const unsigned char *t = sqlite3_column_text(st, col);
    if (!t) { if (dst_sz) dst[0] = '\0'; return; }
    UTAX_STRNCPY(dst, dst_sz, (const char *)t);
}

static utax_rc utax__build_where_plain(char *sql, size_t sql_sz, const utax_options_filter *f) {
    if (!f) return UTAX_OK;

    if (f->has_year) {
        if (f->year_mode == UTAX_YEAR_UP_TO) {
            if (!UTAX_STRCAT(sql, sql_sz, " AND bought_year <= ?")) return UTAX_ERR_INVALID_ARG;
        } else {
            if (!UTAX_STRCAT(sql, sql_sz, " AND bought_year = ?")) return UTAX_ERR_INVALID_ARG;
        }
    }
    if (f->has_broker) { if (!UTAX_STRCAT(sql, sql_sz, " AND broker = ?")) return UTAX_ERR_INVALID_ARG; }
    if (f->has_ticker) { if (!UTAX_STRCAT(sql, sql_sz, " AND ticker = ?")) return UTAX_ERR_INVALID_ARG; }

    return UTAX_OK;
}

static utax_rc utax__bind_filters(struct utax_db *h, sqlite3_stmt *st, const utax_options_filter *f, int *io_idx) {
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

    *io_idx = idx;
    return UTAX_OK;
}

static utax_rc utax__bind_pagination(struct utax_db *h, sqlite3_stmt *st, const utax_options_filter *f, int *io_idx) {
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

utax_rc utax_options_insert(utax_db_t *db, const utax_options_row *row, long long *out_id) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "INSERT INTO options_operations ("
        " broker, bought_dt, expiration_dt, ticker, amount_x100, per_contract, tax, country, currency, conversion_rate_eur"
        ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10);";

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    (void)utax__bind_text(st, 1, row->broker);
    (void)utax__bind_text(st, 2, row->bought_dt);
    (void)utax__bind_text(st, 3, row->expiration_dt);
    (void)utax__bind_text(st, 4, row->ticker);
    sqlite3_bind_int(st, 5, row->amount_x100);
    sqlite3_bind_double(st, 6, row->per_contract);
    sqlite3_bind_double(st, 7, row->tax);
    (void)utax__bind_text(st, 8, row->country);
    (void)utax__bind_text(st, 9, row->currency);
    sqlite3_bind_double(st, 10, row->conversion_rate_eur);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);
    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (out_id) *out_id = (long long)sqlite3_last_insert_rowid(h->db);
    return UTAX_OK;
}

utax_rc utax_options_insert_many(utax_db_t *db, utax_options_row *rows, size_t n, size_t *out_inserted) {
    if (!db || (!rows && n != 0)) return UTAX_ERR_INVALID_ARG;
    if (out_inserted) *out_inserted = 0;
    if (n == 0) return UTAX_OK;

    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "INSERT INTO options_operations ("
        " broker, bought_dt, expiration_dt, ticker, amount_x100, per_contract, tax, country, currency, conversion_rate_eur"
        ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10);";

    int rc0 = sqlite3_exec(h->db, "BEGIN;", NULL, NULL, NULL);
    if (rc0 != SQLITE_OK) return utax__set_err_sqlite(h, rc0);

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) {
        (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
        return rc;
    }

    size_t i = 0;
    for (; i < n; ++i) {
        utax_options_row *r = &rows[i];

        sqlite3_clear_bindings(st);
        sqlite3_reset(st);

        (void)utax__bind_text(st, 1, r->broker);
        (void)utax__bind_text(st, 2, r->bought_dt);
        (void)utax__bind_text(st, 3, r->expiration_dt);
        (void)utax__bind_text(st, 4, r->ticker);
        sqlite3_bind_int(st, 5, r->amount_x100);
        sqlite3_bind_double(st, 6, r->per_contract);
        sqlite3_bind_double(st, 7, r->tax);
        (void)utax__bind_text(st, 8, r->country);
        (void)utax__bind_text(st, 9, r->currency);
        sqlite3_bind_double(st, 10, r->conversion_rate_eur);

        int s = sqlite3_step(st);
        if (s != SQLITE_DONE) {
            sqlite3_finalize(st);
            (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
            if (out_inserted) *out_inserted = i;
            return utax__set_err_sqlite(h, s);
        }

        r->option_id = (long long)sqlite3_last_insert_rowid(h->db);
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

utax_rc utax_options_update_by_id(utax_db_t *db, long long id, const utax_options_row *row) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "UPDATE options_operations SET "
        " broker=?1, bought_dt=?2, expiration_dt=?3, ticker=?4, amount_x100=?5, per_contract=?6, tax=?7, country=?8, currency=?9, conversion_rate_eur=?10 "
        "WHERE option_id=?11;";

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    (void)utax__bind_text(st, 1, row->broker);
    (void)utax__bind_text(st, 2, row->bought_dt);
    (void)utax__bind_text(st, 3, row->expiration_dt);
    (void)utax__bind_text(st, 4, row->ticker);
    sqlite3_bind_int(st, 5, row->amount_x100);
    sqlite3_bind_double(st, 6, row->per_contract);
    sqlite3_bind_double(st, 7, row->tax);
    (void)utax__bind_text(st, 8, row->country);
    (void)utax__bind_text(st, 9, row->currency);
    sqlite3_bind_double(st, 10, row->conversion_rate_eur);
    sqlite3_bind_int64(st, 11, (sqlite3_int64)id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);
    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "option id not found");
        return UTAX_ERR_NOT_FOUND;
    }
    return UTAX_OK;
}

utax_rc utax_options_delete_by_id(utax_db_t *db, long long id) {
    if (!db) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, "DELETE FROM options_operations WHERE option_id=?1;");
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_int64(st, 1, (sqlite3_int64)id);
    int s = sqlite3_step(st);
    sqlite3_finalize(st);
    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "option id not found");
        return UTAX_ERR_NOT_FOUND;
    }
    return UTAX_OK;
}

utax_rc utax_options_count_total(utax_db_t *db, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, "SELECT COUNT(*) FROM options_operations;");
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

utax_rc utax_options_count_filtered(utax_db_t *db, const utax_options_filter *f, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[768];
    UTAX_STRNCPY(sql, sizeof(sql), "SELECT COUNT(*) FROM options_operations WHERE 1=1");

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

utax_rc utax_options_count_page(utax_db_t *db, const utax_options_filter *f, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[1024];
    UTAX_STRNCPY(sql, sizeof(sql), "SELECT COUNT(*) FROM (SELECT option_id FROM options_operations WHERE 1=1");

    utax_rc rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY bought_dt ASC, expiration_dt ASC, option_id ASC")) return UTAX_ERR_INVALID_ARG;

    if (f && (f->has_limit || f->has_offset)) {
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

utax_rc utax_options_get_filtered(utax_db_t *db,
                                  const utax_options_filter *f,
                                  utax_options_row *out_rows,
                                  size_t out_cap,
                                  size_t *out_count,
                                  size_t *out_required) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    if (!out_rows && out_cap != 0) return UTAX_ERR_INVALID_ARG;

    *out_count = 0;
    if (out_required) *out_required = 0;

    struct utax_db *h = (struct utax_db *)db;

    long long needed_ll = 0;
    utax_rc rc = utax_options_count_page(db, f, &needed_ll);
    if (rc != UTAX_OK) return rc;

    size_t needed = (needed_ll <= 0) ? 0 : (size_t)needed_ll;
    if (out_required) *out_required = needed;
    if (needed > out_cap) {
        utax__set_err_msg(h, "output array too small");
        return UTAX_ERR_NO_SPACE;
    }

    char sql[1200];
    UTAX_STRNCPY(sql, sizeof(sql),
        "SELECT "
        " option_id, broker, bought_dt, expiration_dt, bought_year, ticker, amount_x100, per_contract, tax, country, currency, conversion_rate_eur "
        "FROM options_operations WHERE 1=1"
    );

    rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY bought_dt ASC, expiration_dt ASC, option_id ASC")) return UTAX_ERR_INVALID_ARG;

    if (f && (f->has_limit || f->has_offset)) {
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
        utax_options_row *r = &out_rows[i++];
        r->option_id = (long long)sqlite3_column_int64(st, 0);
        utax__col_text(st, 1, r->broker, sizeof(r->broker));
        utax__col_text(st, 2, r->bought_dt, sizeof(r->bought_dt));
        utax__col_text(st, 3, r->expiration_dt, sizeof(r->expiration_dt));
        r->bought_year = sqlite3_column_int(st, 4);
        utax__col_text(st, 5, r->ticker, sizeof(r->ticker));
        r->amount_x100 = sqlite3_column_int(st, 6);
        r->per_contract = sqlite3_column_double(st, 7);
        r->tax = sqlite3_column_double(st, 8);
        utax__col_text(st, 9, r->country, sizeof(r->country));
        utax__col_text(st, 10, r->currency, sizeof(r->currency));
        r->conversion_rate_eur = sqlite3_column_double(st, 11);
    }

    sqlite3_finalize(st);
    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    *out_count = i;
    return UTAX_OK;
}

static char *utax__trim_ws(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';
    return s;
}

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

static utax_rc utax__parse_int_strict(const char *s, int *out) {
    if (!s || !out) return UTAX_ERR_INVALID_ARG;

    errno = 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);

    if (end == s) return UTAX_ERR_BAD_FIELD;
    while (end && *end && isspace((unsigned char)*end)) end++;
    if (end && *end) return UTAX_ERR_BAD_FIELD;
    if (errno == ERANGE) return UTAX_ERR_OVERFLOW;
    if (v < (long)INT_MIN || v > (long)INT_MAX) return UTAX_ERR_OVERFLOW;

    *out = (int)v;
    return UTAX_OK;
}

static utax_rc utax__normalize_date_to_dt(const char *date10, char out_dt[UTAX_DT_MAX]) {
    if (!date10) return UTAX_ERR_BAD_FIELD;
    if (strlen(date10) != 10 || date10[4] != '-' || date10[7] != '-') return UTAX_ERR_BAD_FIELD;
    int n = snprintf(out_dt, UTAX_DT_MAX, "%s 00:00", date10);
    return (n == 16) ? UTAX_OK : UTAX_ERR_BAD_FIELD;
}

static utax_rc utax__validate_options_header(char *hdr_line) {
    static const char *k_expected[10] = {
        "BOUGHT_DATE","BROKER","EXPIRATION","TICKER","AMOUNT_X100","PER_CONTRACT","TAX","COUNTRY","CURRENCY","CONVERSION_RATE_1EUR"
    };

    char *fields[16] = {0};
    int nf = utax__split_csv_simple(hdr_line, fields, 16);
    if (nf != 10) return UTAX_ERR_BAD_HEADER;

    for (int i = 0; i < nf; ++i) fields[i] = utax__trim_ws(fields[i]);
    for (int i = 0; i < 10; ++i) {
        if (strcmp(fields[i], k_expected[i]) != 0) return UTAX_ERR_BAD_HEADER;
    }
    return UTAX_OK;
}

utax_rc utax_options_parse_csv_file(const char *path,
                                    utax_options_row **inout_rows,
                                    size_t *inout_total_elems) {
    if (!path || !inout_rows || !inout_total_elems) return UTAX_ERR_INVALID_ARG;

    FILE *f = NULL;
    if (!UTAX_FOPEN(f, path, "rb")) return UTAX_ERR_IO_OPEN;

    utax_options_row *parsed_rows = NULL;
    size_t parsed_count = 0;
    size_t parsed_cap = 0;

    char line[4096];

    if (!fgets(line, sizeof(line), f)) {
        if (ferror(f)) { fclose(f); return UTAX_ERR_IO_READ; }
        fclose(f);
        return UTAX_OK;
    }

    char *nl = strpbrk(line, "\r\n");
    if (nl) *nl = '\0';

    char *hdr = utax__trim_ws(line);
    if (!strchr(hdr, ',')) { fclose(f); return UTAX_ERR_UNSUPPORTED; }

    utax_rc hrc = utax__validate_options_header(hdr);
    if (hrc != UTAX_OK) { fclose(f); return hrc; }

    while (fgets(line, sizeof(line), f)) {
        char *nl2 = strpbrk(line, "\r\n");
        if (nl2) *nl2 = '\0';

        char *s = utax__trim_ws(line);
        if (!*s) continue;

        char *fields[16] = {0};
        int nf = utax__split_csv_simple(s, fields, 16);
        if (nf != 10) {
            fclose(f);
            free(parsed_rows);
            return UTAX_ERR_PARSE;
        }

        for (int i = 0; i < nf; ++i) fields[i] = utax__trim_ws(fields[i]);

        if (parsed_count == parsed_cap) {
            size_t new_cap = (parsed_cap == 0) ? 8 : (parsed_cap * 2);
            utax_options_row *grown = (utax_options_row *)realloc(parsed_rows, new_cap * sizeof(*grown));
            if (!grown) {
                fclose(f);
                free(parsed_rows);
                return UTAX_ERR_NOMEM;
            }
            parsed_rows = grown;
            parsed_cap = new_cap;
        }

        utax_options_row *r = &parsed_rows[parsed_count];
        memset(r, 0, sizeof(*r));

        utax_rc rc_bdt = utax__normalize_date_to_dt(fields[0], r->bought_dt);
        utax_rc rc_edt = utax__normalize_date_to_dt(fields[2], r->expiration_dt);
        if (rc_bdt != UTAX_OK || rc_edt != UTAX_OK) {
            fclose(f);
            free(parsed_rows);
            return UTAX_ERR_BAD_FIELD;
        }

        utax_rc rc1 = utax__copy_field(r->broker, sizeof(r->broker), fields[1]);
        utax_rc rc2 = utax__copy_field(r->ticker, sizeof(r->ticker), fields[3]);
        utax_rc rc3 = utax__copy_field(r->country, sizeof(r->country), fields[7]);
        utax_rc rc4 = utax__copy_field(r->currency, sizeof(r->currency), fields[8]);
        if (rc1 == UTAX_ERR_TRUNCATED || rc2 == UTAX_ERR_TRUNCATED || rc3 == UTAX_ERR_TRUNCATED || rc4 == UTAX_ERR_TRUNCATED) {
            fclose(f);
            free(parsed_rows);
            return UTAX_ERR_TRUNCATED;
        }
        if (rc1 != UTAX_OK || rc2 != UTAX_OK || rc3 != UTAX_OK || rc4 != UTAX_OK) {
            fclose(f);
            free(parsed_rows);
            return UTAX_ERR_PARSE;
        }

        utax_rc rca = utax__parse_int_strict(fields[4], &r->amount_x100);
        utax_rc rcp = utax__parse_double_strict(fields[5], &r->per_contract);
        utax_rc rct = utax__parse_double_strict(fields[6], &r->tax);
        utax_rc rcr = utax__parse_double_strict(fields[9], &r->conversion_rate_eur);
        if (rca == UTAX_ERR_OVERFLOW || rcp == UTAX_ERR_OVERFLOW || rct == UTAX_ERR_OVERFLOW || rcr == UTAX_ERR_OVERFLOW) {
            fclose(f);
            free(parsed_rows);
            return UTAX_ERR_OVERFLOW;
        }
        if (rca != UTAX_OK || rcp != UTAX_OK || rct != UTAX_OK || rcr != UTAX_OK) {
            fclose(f);
            free(parsed_rows);
            return UTAX_ERR_BAD_FIELD;
        }

        r->option_id = 0;
        r->bought_year = 0;
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
        utax_options_row *base_rows = *inout_rows;
        size_t total = base_count + parsed_count;
        utax_options_row *grown = (utax_options_row *)realloc(base_rows, total * sizeof(*grown));
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

void utax_options_free_rows(utax_options_row **inout_rows, size_t *inout_total_elems) {
    if (!inout_rows || !inout_total_elems) return;
    free(*inout_rows);
    *inout_rows = NULL;
    *inout_total_elems = 0;
}

utax_rc utax_options_insert_many_array(utax_db_t *db, utax_options_row *rows, size_t n, size_t *out_inserted) {
    return utax_options_insert_many(db, rows, n, out_inserted);
}

utax_rc utax_options_insert_many_from_csv_file(utax_db_t *db, const char *path, size_t *out_inserted) {
    if (!db || !path) return UTAX_ERR_INVALID_ARG;
    if (out_inserted) *out_inserted = 0;

    utax_options_row *rows = NULL;
    size_t total = 0;

    utax_rc rc = utax_options_parse_csv_file(path, &rows, &total);
    if (rc != UTAX_OK) {
        utax_options_free_rows(&rows, &total);
        return rc;
    }

    size_t inserted = 0;
    rc = utax_options_insert_many_array(db, rows, total, &inserted);
    utax_options_free_rows(&rows, &total);

    if (out_inserted) *out_inserted = inserted;
    return rc;
}
