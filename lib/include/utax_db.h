#ifndef ULTIMATETAX_UTAX_DB_H
#define ULTIMATETAX_UTAX_DB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* ---- Export macros (Windows/Linux/macOS) ---- */
#if defined(_WIN32) || defined(__CYGWIN__)
  /* Static library is the default. Define UTAX_DLL only if you build/consume a DLL. */
  #if defined(UTAX_DLL)
    #if defined(UTAX_BUILD_DLL)
      #define UTAX_API __declspec(dllexport)
    #else
      #define UTAX_API __declspec(dllimport)
    #endif
  #else
    #define UTAX_API
  #endif
#else
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define UTAX_API __attribute__((visibility("default")))
  #else
    #define UTAX_API
  #endif
#endif

/* Opaque DB handle */
typedef struct utax_db utax_db_t;

/* Return codes */
typedef enum utax_rc {
    UTAX_OK = 0,
    UTAX_ERR_INVALID_ARG = 1,
    UTAX_ERR_NOMEM = 2,
    UTAX_ERR_SQLITE = 3,
    UTAX_ERR_NOT_FOUND = 4,
    UTAX_ERR_NO_SPACE  = 5
} utax_rc;

typedef struct utax_db_open_opts {
    int create_if_missing;   /* default 1 */
    int read_only;           /* default 0 */
    int busy_timeout_ms;     /* default 5000 */
} utax_db_open_opts;

/* Defaults */
UTAX_API utax_db_open_opts utax_db_open_opts_default(void);

/* Open database at path */
UTAX_API utax_rc utax_db_open(const char *path,
                              const utax_db_open_opts *opts,
                              utax_db_t **out_db);

/* Close database */
UTAX_API utax_rc utax_db_close(utax_db_t *db);

/* Last error string (owned by handle; valid until close) */
UTAX_API const char *utax_db_last_error(const utax_db_t *db);

#ifdef __cplusplus
}
#endif

#endif /* ULTIMATETAX_UTAX_DB_H */
