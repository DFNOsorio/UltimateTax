#include "utax_db_priv.h"

#include <stdlib.h>


static void utax__clear_err(struct utax_db *h) {
    if (!h) return;
    h->last_errmsg[0] = '\0';
}

utax_rc utax__set_err_sqlite(struct utax_db *h, int sqlite_rc) {
    if (!h) return UTAX_ERR_SQLITE;

    const char *msg = (h->db != NULL) ? sqlite3_errmsg(h->db) : "sqlite error";
    UTAX_STRNCPY(h->last_errmsg, sizeof(h->last_errmsg), msg);
    h->last_errmsg[sizeof(h->last_errmsg) - 1] = '\0';

    (void)sqlite_rc; /* reserved for future mapping */
    return UTAX_ERR_SQLITE;
}

utax_db_open_opts utax_db_open_opts_default(void) {
    utax_db_open_opts o;
    o.create_if_missing = 1;
    o.read_only = 0;
    o.busy_timeout_ms = 5000;
    return o;
}

utax_rc utax_db_open(const char *path,
                     const utax_db_open_opts *opts,
                     utax_db_t **out_db) {
    if (!path || !out_db) return UTAX_ERR_INVALID_ARG;
    *out_db = NULL;

    utax_db_open_opts o = opts ? *opts : utax_db_open_opts_default();

    struct utax_db *h = (struct utax_db *)calloc(1, sizeof(struct utax_db));
    if (!h) return UTAX_ERR_NOMEM;

    utax__clear_err(h);

    int flags = 0;
    if (o.read_only) {
        flags = SQLITE_OPEN_READONLY;
    } else {
        flags = SQLITE_OPEN_READWRITE;
        if (o.create_if_missing) flags |= SQLITE_OPEN_CREATE;
    }

    int rc = sqlite3_open_v2(path, &h->db, flags, NULL);
    if (rc != SQLITE_OK) {
        utax__set_err_sqlite(h, rc);
        if (h->db) sqlite3_close_v2(h->db);
        free(h);
        return UTAX_ERR_SQLITE;
    }

    if (o.busy_timeout_ms > 0) {
        sqlite3_busy_timeout(h->db, o.busy_timeout_ms);
    }

    char *errmsg = NULL;
    int prc = sqlite3_exec(h->db, "PRAGMA foreign_keys=ON;", NULL, NULL, &errmsg);
    if (prc != SQLITE_OK) {
        if (errmsg) {
            UTAX_STRNCPY(h->last_errmsg, sizeof(h->last_errmsg), errmsg);
            sqlite3_free(errmsg);
        } else {
            utax__set_err_sqlite(h, prc);
        }
        sqlite3_close_v2(h->db);
        free(h);
        return UTAX_ERR_SQLITE;
    }

    *out_db = (utax_db_t *)h;
    return UTAX_OK;
}

utax_rc utax_db_close(utax_db_t *db) {
    if (!db) return UTAX_ERR_INVALID_ARG;

    struct utax_db *h = (struct utax_db *)db;
    if (h->db) {
        sqlite3_close_v2(h->db);
        h->db = NULL;
    }
    free(h);
    return UTAX_OK;
}

const char *utax_db_last_error(const utax_db_t *db) {
    if (!db) return "invalid db handle";
    const struct utax_db *h = (const struct utax_db *)db;
    return h->last_errmsg[0] ? h->last_errmsg : "";
}
