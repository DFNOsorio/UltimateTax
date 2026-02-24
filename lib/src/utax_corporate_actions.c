#include "utax_corporate_actions.h"
#include "utax_db_priv.h"

#include <sqlite3.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

static void utax__col_text(sqlite3_stmt *st, int col, char *dst, size_t dst_sz) {
    const unsigned char *t = sqlite3_column_text(st, col);
    if (!t) { if (dst_sz) dst[0] = '\0'; return; }
    UTAX_STRNCPY(dst, dst_sz, (const char *)t);
}

static utax_rc utax__bind_text_nullable(sqlite3_stmt *st, int idx, const char *s) {
    if (!s || s[0] == '\0') {
        return (sqlite3_bind_null(st, idx) == SQLITE_OK) ? UTAX_OK : UTAX_ERR_SQLITE;
    }
    return (sqlite3_bind_text(st, idx, s, -1, SQLITE_TRANSIENT) == SQLITE_OK) ? UTAX_OK : UTAX_ERR_SQLITE;
}

static utax_rc utax__build_where_plain(char *sql, size_t sql_sz, const utax_corporate_actions_filter *f) {
    if (!f) return UTAX_OK;

    if (f->has_year) {
        if (f->year_mode == UTAX_YEAR_UP_TO) {
            if (!UTAX_STRCAT(sql, sql_sz, " AND action_year <= ?")) return UTAX_ERR_INVALID_ARG;
        } else {
            if (!UTAX_STRCAT(sql, sql_sz, " AND action_year = ?")) return UTAX_ERR_INVALID_ARG;
        }
    }
    if (f->has_broker) {
        if (!UTAX_STRCAT(sql, sql_sz, " AND broker = ?")) return UTAX_ERR_INVALID_ARG;
    }
    if (f->has_from_ticker) {
        if (!UTAX_STRCAT(sql, sql_sz, " AND from_ticker = ?")) return UTAX_ERR_INVALID_ARG;
    }

    return UTAX_OK;
}

static utax_rc utax__bind_filters(struct utax_db *h, sqlite3_stmt *st, const utax_corporate_actions_filter *f, int *io_idx) {
    int idx = *io_idx;
    if (!f) return UTAX_OK;

    if (f->has_year) {
        if (sqlite3_bind_int(st, idx++, f->year) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f->has_broker) {
        if (sqlite3_bind_text(st, idx++, f->broker, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f->has_from_ticker) {
        if (sqlite3_bind_text(st, idx++, f->from_ticker, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }

    *io_idx = idx;
    return UTAX_OK;
}

static utax_rc utax__bind_pagination(struct utax_db *h, sqlite3_stmt *st, const utax_corporate_actions_filter *f, int *io_idx) {
    if (!f) return UTAX_OK;

    if (f->has_limit || f->has_offset) {
        int limit  = f->has_limit  ? f->limit  : -1;
        int offset = f->has_offset ? f->offset : 0;

        if (limit < -1 || offset < 0) {
            utax__set_err_msg(h, "invalid pagination");
            return UTAX_ERR_INVALID_ARG;
        }

        if (sqlite3_bind_int(st, (*io_idx)++, limit) != SQLITE_OK)  return utax__set_err_sqlite(h, SQLITE_ERROR);
        if (sqlite3_bind_int(st, (*io_idx)++, offset) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    return UTAX_OK;
}

/* ───────────────────────────── CRUD ───────────────────────────── */

utax_rc utax_corporate_actions_insert(utax_db_t *db, const utax_corporate_actions_row *row, long long *out_action_id) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "INSERT INTO corporate_actions ("
        " broker, action_date, action_type, from_ticker, to_ticker, from_qty, to_qty"
        ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);";

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_text(st, 1, row->broker, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, row->action_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, row->action_type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, row->from_ticker, -1, SQLITE_TRANSIENT);

    /* SPLIT => to_ticker must be NULL */
    if (strcmp(row->action_type, "SPLIT") == 0) {
        sqlite3_bind_null(st, 5);
    } else {
        (void)utax__bind_text_nullable(st, 5, row->to_ticker);
    }

    sqlite3_bind_double(st, 6, row->from_qty);
    sqlite3_bind_double(st, 7, row->to_qty);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (out_action_id) *out_action_id = (long long)sqlite3_last_insert_rowid(h->db);
    return UTAX_OK;
}

utax_rc utax_corporate_actions_update_by_id(utax_db_t *db, long long action_id, const utax_corporate_actions_row *row) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "UPDATE corporate_actions SET "
        " broker=?1, action_date=?2, action_type=?3, from_ticker=?4, to_ticker=?5, from_qty=?6, to_qty=?7 "
        "WHERE action_id=?8;";

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_text(st, 1, row->broker, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, row->action_date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, row->action_type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, row->from_ticker, -1, SQLITE_TRANSIENT);

    if (strcmp(row->action_type, "SPLIT") == 0) sqlite3_bind_null(st, 5);
    else (void)utax__bind_text_nullable(st, 5, row->to_ticker);

    sqlite3_bind_double(st, 6, row->from_qty);
    sqlite3_bind_double(st, 7, row->to_qty);

    sqlite3_bind_int64(st, 8, (sqlite3_int64)action_id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "corporate action id not found");
        return UTAX_ERR_NOT_FOUND;
    }

    return UTAX_OK;
}

utax_rc utax_corporate_actions_delete_by_id(utax_db_t *db, long long action_id) {
    if (!db) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, "DELETE FROM corporate_actions WHERE action_id=?1;");
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_int64(st, 1, (sqlite3_int64)action_id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "corporate action id not found");
        return UTAX_ERR_NOT_FOUND;
    }

    return UTAX_OK;
}

/* ───────────────────────────── Counts ───────────────────────────── */

utax_rc utax_corporate_actions_count_total(utax_db_t *db, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, "SELECT COUNT(*) FROM corporate_actions;");
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

utax_rc utax_corporate_actions_count_filtered(utax_db_t *db, const utax_corporate_actions_filter *f, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[768];
    UTAX_STRNCPY(sql, sizeof(sql), "SELECT COUNT(*) FROM corporate_actions WHERE 1=1");

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

utax_rc utax_corporate_actions_count_page(utax_db_t *db, const utax_corporate_actions_filter *f, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[1024];
    UTAX_STRNCPY(sql, sizeof(sql),
        "SELECT COUNT(*) FROM ("
        " SELECT action_id FROM corporate_actions WHERE 1=1"
    );

    utax_rc rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY action_date ASC, action_id ASC")) return UTAX_ERR_INVALID_ARG;

    if (f && (f->has_limit || f->has_offset)) {
        if (!UTAX_STRCAT(sql, sizeof(sql), " LIMIT ? OFFSET ?")) return UTAX_ERR_INVALID_ARG;
    }

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

/* ───────────────────────────── Query rows ───────────────────────────── */

utax_rc utax_corporate_actions_get_filtered(utax_db_t *db,
                                            const utax_corporate_actions_filter *f,
                                            utax_corporate_actions_row *out_rows,
                                            size_t out_cap,
                                            size_t *out_count,
                                            size_t *out_required)
{
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    if (!out_rows && out_cap != 0) return UTAX_ERR_INVALID_ARG;

    *out_count = 0;
    if (out_required) *out_required = 0;

    struct utax_db *h = (struct utax_db *)db;

    long long needed_ll = 0;
    utax_rc rc = utax_corporate_actions_count_page(db, f, &needed_ll);
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
        " action_id, broker, action_date, action_year, action_type, "
        " from_ticker, to_ticker, from_qty, to_qty, ratio "
        "FROM corporate_actions WHERE 1=1"
    );

    rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY action_date ASC, action_id ASC")) return UTAX_ERR_INVALID_ARG;

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
        utax_corporate_actions_row *r = &out_rows[i++];

        r->action_id = (long long)sqlite3_column_int64(st, 0);
        utax__col_text(st, 1, r->broker, sizeof(r->broker));
        utax__col_text(st, 2, r->action_date, sizeof(r->action_date));
        r->action_year = sqlite3_column_int(st, 3);
        utax__col_text(st, 4, r->action_type, sizeof(r->action_type));

        utax__col_text(st, 5, r->from_ticker, sizeof(r->from_ticker));
        utax__col_text(st, 6, r->to_ticker, sizeof(r->to_ticker)); /* becomes "" if NULL */
        r->from_qty = sqlite3_column_double(st, 7);
        r->to_qty   = sqlite3_column_double(st, 8);
        r->ratio    = sqlite3_column_double(st, 9);
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

/* Simple CSV splitter (no quotes). */
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

static utax_rc utax__validate_iso_date_yyyy_mm_dd(const char *d) {
    if (!d) return UTAX_ERR_BAD_FIELD;
    if (strlen(d) != 10) return UTAX_ERR_BAD_FIELD;
    if (d[4] != '-' || d[7] != '-') return UTAX_ERR_BAD_FIELD;
    return UTAX_OK;
}

static int utax__is_action_type_ok(const char *t) {
    return t &&
           (strcmp(t, "MERGER") == 0 ||
            strcmp(t, "CONVERSION") == 0 ||
            strcmp(t, "SPINOFF") == 0 ||
            strcmp(t, "SPLIT") == 0);
}

static utax_rc utax__validate_ca_header(char *hdr_line) {
    static const char *k_expected[7] = {
        "ACTION_DATE","BROKER","ACTION_TYPE","FROM_TICKER","FROM_QTY","TO_TICKER","TO_QTY"
    };

    char *fields[16] = {0};
    int nf = utax__split_csv_simple(hdr_line, fields, 16);
    if (nf != 7) return UTAX_ERR_BAD_HEADER;

    for (int i = 0; i < nf; ++i) fields[i] = utax__trim_ws(fields[i]);

    for (int i = 0; i < 7; ++i) {
        if (strcmp(fields[i], k_expected[i]) != 0) return UTAX_ERR_BAD_HEADER;
    }
    return UTAX_OK;
}

utax_rc utax_corporate_actions_parse_csv_file(const char *path,
                                              utax_corporate_actions_node **inout_head,
                                              size_t *inout_total_elems)
{
    if (!path || !inout_head || !inout_total_elems) return UTAX_ERR_INVALID_ARG;

    FILE *f = NULL;
    if (!UTAX_FOPEN(f, path, "rb")) return UTAX_ERR_IO_OPEN;

    utax_corporate_actions_node *tail = *inout_head;
    while (tail && tail->next) tail = tail->next;

    utax_corporate_actions_node *new_head = NULL;
    utax_corporate_actions_node *new_tail = NULL;

    char line[4096];

    /* header */
    if (!fgets(line, sizeof(line), f)) {
        if (ferror(f)) { fclose(f); return UTAX_ERR_IO_READ; }
        fclose(f);
        return UTAX_OK;
    }

    char *nl = strpbrk(line, "\r\n");
    if (nl) *nl = '\0';

    char *hdr = utax__trim_ws(line);
    if (!strchr(hdr, ',')) { fclose(f); return UTAX_ERR_UNSUPPORTED; }

    utax_rc hrc = utax__validate_ca_header(hdr);
    if (hrc != UTAX_OK) { fclose(f); return hrc; }

    while (fgets(line, sizeof(line), f)) {
        char *nl2 = strpbrk(line, "\r\n");
        if (nl2) *nl2 = '\0';

        char *s = utax__trim_ws(line);
        if (!*s) continue;

        char *fields[16] = {0};
        int nf = utax__split_csv_simple(s, fields, 16);
        if (nf != 7) {
            size_t tmp = 0;
            utax_corporate_actions_free_list(&new_head, &tmp);
            fclose(f);
            return UTAX_ERR_PARSE;
        }
        for (int i = 0; i < nf; ++i) fields[i] = utax__trim_ws(fields[i]);

        utax_rc rcd = utax__validate_iso_date_yyyy_mm_dd(fields[0]);
        if (rcd != UTAX_OK) {
            size_t tmp = 0;
            utax_corporate_actions_free_list(&new_head, &tmp);
            fclose(f);
            return rcd;
        }

        if (!utax__is_action_type_ok(fields[2])) {
            size_t tmp = 0;
            utax_corporate_actions_free_list(&new_head, &tmp);
            fclose(f);
            return UTAX_ERR_BAD_FIELD;
        }

        utax_corporate_actions_node *node = (utax_corporate_actions_node *)calloc(1, sizeof(*node));
        if (!node) {
            size_t tmp = 0;
            utax_corporate_actions_free_list(&new_head, &tmp);
            fclose(f);
            return UTAX_ERR_NOMEM;
        }

        utax_corporate_actions_row *r = &node->row;
        memset(r, 0, sizeof(*r));

        utax_rc t0 = utax__copy_field(r->action_date, sizeof(r->action_date), fields[0]);
        utax_rc t1 = utax__copy_field(r->broker, sizeof(r->broker), fields[1]);
        utax_rc t2 = utax__copy_field(r->action_type, sizeof(r->action_type), fields[2]);
        utax_rc t3 = utax__copy_field(r->from_ticker, sizeof(r->from_ticker), fields[3]);

        if (t0 == UTAX_ERR_TRUNCATED || t1 == UTAX_ERR_TRUNCATED || t2 == UTAX_ERR_TRUNCATED || t3 == UTAX_ERR_TRUNCATED) {
            free(node);
            size_t tmp = 0;
            utax_corporate_actions_free_list(&new_head, &tmp);
            fclose(f);
            return UTAX_ERR_TRUNCATED;
        }
        if (t0 != UTAX_OK || t1 != UTAX_OK || t2 != UTAX_OK || t3 != UTAX_OK) {
            free(node);
            size_t tmp = 0;
            utax_corporate_actions_free_list(&new_head, &tmp);
            fclose(f);
            return UTAX_ERR_PARSE;
        }

        utax_rc rq1 = utax__parse_double_strict(fields[4], &r->from_qty);
        utax_rc rq2 = utax__parse_double_strict(fields[6], &r->to_qty);
        if (rq1 == UTAX_ERR_OVERFLOW || rq2 == UTAX_ERR_OVERFLOW) {
            free(node);
            size_t tmp = 0;
            utax_corporate_actions_free_list(&new_head, &tmp);
            fclose(f);
            return UTAX_ERR_OVERFLOW;
        }
        if (rq1 != UTAX_OK || rq2 != UTAX_OK || r->from_qty <= 0.0 || r->to_qty <= 0.0) {
            free(node);
            size_t tmp = 0;
            utax_corporate_actions_free_list(&new_head, &tmp);
            fclose(f);
            return UTAX_ERR_BAD_FIELD;
        }

        /* SPLIT rules: to_ticker must be empty; others must have to_ticker */
        if (strcmp(r->action_type, "SPLIT") == 0) {
            if (fields[5] && fields[5][0] != '\0') {
                free(node);
                size_t tmp = 0;
                utax_corporate_actions_free_list(&new_head, &tmp);
                fclose(f);
                return UTAX_ERR_BAD_FIELD;
            }
            r->to_ticker[0] = '\0';
        } else {
            if (!fields[5] || fields[5][0] == '\0') {
                free(node);
                size_t tmp = 0;
                utax_corporate_actions_free_list(&new_head, &tmp);
                fclose(f);
                return UTAX_ERR_BAD_FIELD;
            }
            utax_rc t4 = utax__copy_field(r->to_ticker, sizeof(r->to_ticker), fields[5]);
            if (t4 == UTAX_ERR_TRUNCATED) {
                free(node);
                size_t tmp = 0;
                utax_corporate_actions_free_list(&new_head, &tmp);
                fclose(f);
                return UTAX_ERR_TRUNCATED;
            }
            if (t4 != UTAX_OK) {
                free(node);
                size_t tmp = 0;
                utax_corporate_actions_free_list(&new_head, &tmp);
                fclose(f);
                return UTAX_ERR_PARSE;
            }
        }

        r->action_id = 0;
        r->action_year = 0;
        r->ratio = 0.0;

        node->next = NULL;
        if (!new_head) new_head = node;
        else new_tail->next = node;
        new_tail = node;
    }

    if (ferror(f)) {
        size_t tmp = 0;
        utax_corporate_actions_free_list(&new_head, &tmp);
        fclose(f);
        return UTAX_ERR_IO_READ;
    }

    fclose(f);

    if (new_head) {
        if (!*inout_head) *inout_head = new_head;
        else tail->next = new_head;

        size_t appended = 0;
        for (utax_corporate_actions_node *p = new_head; p; p = p->next) appended++;
        *inout_total_elems += appended;
    }

    return UTAX_OK;
}

void utax_corporate_actions_free_list(utax_corporate_actions_node **inout_head,
                                      size_t *inout_total_elems)
{
    if (!inout_head || !inout_total_elems) return;

    utax_corporate_actions_node *p = *inout_head;
    while (p) {
        utax_corporate_actions_node *n = p->next;
        free(p);
        p = n;
    }
    *inout_head = NULL;
    *inout_total_elems = 0;
}

static utax_rc utax__ca_step_insert_stmt(struct utax_db *h, sqlite3_stmt *st, utax_corporate_actions_row *r) {
    sqlite3_clear_bindings(st);
    sqlite3_reset(st);

    /* bind: broker, action_date, action_type, from_ticker, to_ticker(NULL for split), from_qty, to_qty */
    (void)utax__bind_text(st, 1, r->broker);
    (void)utax__bind_text(st, 2, r->action_date);
    (void)utax__bind_text(st, 3, r->action_type);
    (void)utax__bind_text(st, 4, r->from_ticker);

    if (strcmp(r->action_type, "SPLIT") == 0) {
        if (sqlite3_bind_null(st, 5) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    } else {
        if (sqlite3_bind_text(st, 5, r->to_ticker, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }

    if (sqlite3_bind_double(st, 6, r->from_qty) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    if (sqlite3_bind_double(st, 7, r->to_qty) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);

    int s = sqlite3_step(st);
    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    r->action_id = (long long)sqlite3_last_insert_rowid(h->db);
    return UTAX_OK;
}

utax_rc utax_corporate_actions_insert_many(utax_db_t *db,
                                          utax_corporate_actions_row *rows,
                                          size_t n,
                                          size_t *out_inserted)
{
    if (!db || (!rows && n != 0)) return UTAX_ERR_INVALID_ARG;
    if (out_inserted) *out_inserted = 0;
    if (n == 0) return UTAX_OK;

    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "INSERT INTO corporate_actions (broker, action_date, action_type, from_ticker, to_ticker, from_qty, to_qty) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);";

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
        rc = utax__ca_step_insert_stmt(h, st, &rows[i]);
        if (rc != UTAX_OK) {
            sqlite3_finalize(st);
            (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
            if (out_inserted) *out_inserted = i;
            return rc;
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

utax_rc utax_corporate_actions_insert_many_list(utax_db_t *db,
                                               utax_corporate_actions_node *head,
                                               size_t *out_inserted)
{
    if (!db) return UTAX_ERR_INVALID_ARG;
    if (out_inserted) *out_inserted = 0;
    if (!head) return UTAX_OK;

    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "INSERT INTO corporate_actions (broker, action_date, action_type, from_ticker, to_ticker, from_qty, to_qty) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);";

    int rc0 = sqlite3_exec(h->db, "BEGIN;", NULL, NULL, NULL);
    if (rc0 != SQLITE_OK) return utax__set_err_sqlite(h, rc0);

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) {
        (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
        return rc;
    }

    size_t i = 0;
    for (utax_corporate_actions_node *p = head; p; p = p->next) {
        rc = utax__ca_step_insert_stmt(h, st, &p->row);
        if (rc != UTAX_OK) {
            sqlite3_finalize(st);
            (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
            if (out_inserted) *out_inserted = i;
            return rc;
        }
        i++;
    }

    sqlite3_finalize(st);

    int rc1 = sqlite3_exec(h->db, "COMMIT;", NULL, NULL, NULL);
    if (rc1 != SQLITE_OK) {
        (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
        if (out_inserted) *out_inserted = i;
        return utax__set_err_sqlite(h, rc1);
    }

    if (out_inserted) *out_inserted = i;
    return UTAX_OK;
}

utax_rc utax_corporate_actions_insert_many_from_csv_file(utax_db_t *db,
                                                        const char *path,
                                                        size_t *out_inserted)
{
    if (!db || !path) return UTAX_ERR_INVALID_ARG;
    if (out_inserted) *out_inserted = 0;

    utax_corporate_actions_node *head = NULL;
    size_t total = 0;

    utax_rc rc = utax_corporate_actions_parse_csv_file(path, &head, &total);
    if (rc != UTAX_OK) {
        utax_corporate_actions_free_list(&head, &total);
        return rc;
    }

    size_t inserted = 0;
    rc = utax_corporate_actions_insert_many_list(db, head, &inserted);

    utax_corporate_actions_free_list(&head, &total);

    if (out_inserted) *out_inserted = inserted;
    return rc;
}
