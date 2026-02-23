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

#endif /* ULTIMATETAX_UTAX_DB_PRIV_H */
