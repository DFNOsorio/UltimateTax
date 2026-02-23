#pragma once

#ifndef ULTIMATETAX_UTAX_DB_PRIV_H
#define ULTIMATETAX_UTAX_DB_PRIV_H

#include "utax_db.h"
#include <sqlite3.h>
#include <string.h>


#if defined(_MSC_VER)
  #define UTAX_STRNCPY(dst, dstsz, src) strncpy_s((dst), (dstsz), (src), _TRUNCATE)
#else
  #define UTAX_STRNCPY(dst, dstsz, src)             \
    do {                                            \
      strncpy((dst), (src), (dstsz) - 1);           \
      (dst)[(dstsz) - 1] = '\0';                    \
    } while (0)
#endif

#if defined(_MSC_VER)
  #define UTAX_FOPEN(out_fp, path, mode) (fopen_s(&(out_fp), (path), (mode)) == 0)
#else
  #define UTAX_FOPEN(out_fp, path, mode) (((out_fp) = fopen((path), (mode))) != NULL)
#endif

struct utax_db {
    sqlite3 *db;
    char last_errmsg[512];
};

#if defined(_MSC_VER)
  #define UTAX_STRCAT(dst, dstsz, src) (strcat_s((dst), (dstsz), (src)) == 0)
#else
  #define UTAX_STRCAT(dst, dstsz, src)                                   \
    (strncat((dst), (src), ((dstsz) - strlen(dst) - 1)) != NULL)
#endif


utax_rc utax__set_err_sqlite(struct utax_db *h, int sqlite_rc);

inline void utax__set_err_msg(struct utax_db *h, const char *msg) {
    if (!h) return;
    UTAX_STRNCPY(h->last_errmsg, sizeof(h->last_errmsg), msg ? msg : "");
}

inline utax_rc utax__prep(struct utax_db *h, sqlite3_stmt **out_st, const char *sql) {
    int rc = sqlite3_prepare_v2(h->db, sql, -1, out_st, NULL);
    if (rc != SQLITE_OK) return utax__set_err_sqlite(h, rc);
    return UTAX_OK;
}

inline utax_rc utax__bind_text(sqlite3_stmt *st, int idx, const char *s) {
    /* Bind empty strings as empty (SQL handles defaults via COALESCE/NULLIF where needed) */
    return (sqlite3_bind_text(st, idx, s ? s : "", -1, SQLITE_TRANSIENT) == SQLITE_OK) ? UTAX_OK : UTAX_ERR_SQLITE;
}

#endif /* ULTIMATETAX_UTAX_DB_PRIV_H */
