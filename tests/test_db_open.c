#include "utax_db.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
  #include <windows.h>
  #include <io.h>
  #define UTAX_ACCESS _access
  #define UTAX_F_OK 0
#else
  #include <unistd.h>
  #define UTAX_ACCESS access
  #define UTAX_F_OK F_OK
#endif

#if defined(_MSC_VER)
  #define UTAX_STRNCPY(dst, dstsz, src) strncpy_s((dst), (dstsz), (src), _TRUNCATE)
#else
  #define UTAX_STRNCPY(dst, dstsz, src)             \
    do {                                            \
      strncpy((dst), (src), (dstsz) - 1);           \
      (dst)[(dstsz) - 1] = '\0';                    \
    } while (0)
#endif

static void make_temp_db_path(char *out, size_t out_sz) {
#if defined(_WIN32)
    char tmpdir[MAX_PATH] = {0};
    DWORD n = GetTempPathA((DWORD)sizeof(tmpdir), tmpdir);
    assert(n > 0 && n < sizeof(tmpdir));

    char tmpfile[MAX_PATH] = {0};
    UINT u = GetTempFileNameA(tmpdir, "utx", 0, tmpfile);
    assert(u != 0);

    /* GetTempFileName creates the file; delete it so sqlite can create it. */
    DeleteFileA(tmpfile);

    UTAX_STRNCPY(out, out_sz, tmpfile);
#else
    snprintf(out, out_sz, "/tmp/utax_test_%ld.db", (long)getpid());
#endif
}

static void test_open_creates_file(void) {
    char path[512];
    make_temp_db_path(path, sizeof(path));

    utax_db_t *db = NULL;
    utax_db_open_opts opts = utax_db_open_opts_default();
    opts.create_if_missing = 1;
    opts.read_only = 0;
    opts.busy_timeout_ms = 50;

    utax_rc rc = utax_db_open(path, &opts, &db);
    assert(rc == UTAX_OK);
    assert(db != NULL);

    rc = utax_db_close(db);
    assert(rc == UTAX_OK);

    /* File should exist on disk */
    assert(UTAX_ACCESS(path, UTAX_F_OK) == 0);

    /* Cleanup */
    remove(path);
}

static void test_open_readonly_missing_fails(void) {
    char path[512];
    make_temp_db_path(path, sizeof(path));

    /* Ensure doesn't exist */
    remove(path);

    utax_db_t *db = NULL;
    utax_db_open_opts opts = utax_db_open_opts_default();
    opts.create_if_missing = 0;
    opts.read_only = 1;
    opts.busy_timeout_ms = 50;

    utax_rc rc = utax_db_open(path, &opts, &db);
    assert(rc != UTAX_OK);
    assert(db == NULL);
}

int main(void) {
    test_open_creates_file();
    test_open_readonly_missing_fails();

    printf("All ultimateTax DB open tests passed.\n");
    return 0;
}
