#include "utax_db.h"
#include "utax_schema.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <sqlite3.h>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <unistd.h>
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

    /* file is created by GetTempFileName; delete it so sqlite can create it */
    DeleteFileA(tmpfile);

    UTAX_STRNCPY(out, out_sz, tmpfile);
#else
    snprintf(out, out_sz, "/tmp/utax_schema_test_%ld.db", (long)getpid());
#endif
}

static int sqlite_table_exists(sqlite3 *db, const char *name) {
    const char *sql =
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1 LIMIT 1;";

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    assert(rc == SQLITE_OK);

    rc = sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
    assert(rc == SQLITE_OK);

    rc = sqlite3_step(st);
    int exists = (rc == SQLITE_ROW);

    sqlite3_finalize(st);
    return exists;
}

static int sqlite_column_exists(sqlite3 *db, const char *table_name, const char *column_name) {
    const char *sql =
        "SELECT 1 FROM pragma_table_xinfo(?1) WHERE name=?2 LIMIT 1;";

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    assert(rc == SQLITE_OK);

    rc = sqlite3_bind_text(st, 1, table_name, -1, SQLITE_STATIC);
    assert(rc == SQLITE_OK);
    rc = sqlite3_bind_text(st, 2, column_name, -1, SQLITE_STATIC);
    assert(rc == SQLITE_OK);

    rc = sqlite3_step(st);
    int exists = (rc == SQLITE_ROW);

    sqlite3_finalize(st);
    return exists;
}

static sqlite3 *sqlite_open_check(const char *path) {
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE, NULL);
    assert(rc == SQLITE_OK);
    assert(db != NULL);
    return db;
}

static void expect_core_tables_exist(sqlite3 *db) {
    assert(sqlite_table_exists(db, "trades"));
    assert(sqlite_table_exists(db, "fifo_snapshot"));
    assert(sqlite_column_exists(db, "fifo_snapshot", "last_price_update_date"));
    assert(sqlite_column_exists(db, "fifo_snapshot", "last_updated_stock_price"));
    assert(sqlite_column_exists(db, "fifo_snapshot", "last_updated_stock_currency"));
    assert(sqlite_column_exists(db, "fifo_snapshot", "last_updated_stock_conversion_rate_eur"));
    assert(sqlite_column_exists(db, "fifo_snapshot", "current_lot_value_eur"));
    assert(sqlite_table_exists(db, "fifo_realized"));
    assert(sqlite_table_exists(db, "dividends"));
    assert(sqlite_table_exists(db, "options_operations"));
}

static void expect_core_tables_absent(sqlite3 *db) {
    assert(!sqlite_table_exists(db, "trades"));
    assert(!sqlite_table_exists(db, "fifo_snapshot"));
    assert(!sqlite_table_exists(db, "fifo_realized"));
    assert(!sqlite_table_exists(db, "dividends"));
    assert(!sqlite_table_exists(db, "options_operations"));
}

int main(int argc, char **argv) {
    /* schema path is passed by CTest */
    assert(argc >= 2);
    const char *schema_path = argv[1];
    assert(schema_path && schema_path[0] != '\0');

    char db_path[512];
    make_temp_db_path(db_path, sizeof(db_path));

    utax_db_t *h = NULL;
    utax_db_open_opts opts = utax_db_open_opts_default();
    opts.create_if_missing = 1;
    opts.read_only = 0;

    utax_rc rc = utax_db_open(db_path, &opts, &h);
    assert(rc == UTAX_OK);
    assert(h != NULL);

    /* Apply schema */
    rc = utax_schema_apply_from_file(h, schema_path);
    if (rc != UTAX_OK) {
        fprintf(stderr, "schema_apply failed rc=%d\nschema=%s\nerr=%s\n",
                (int)rc, schema_path, utax_db_last_error(h));
    }
    assert(rc == UTAX_OK);

    sqlite3 *chk = sqlite_open_check(db_path);
    expect_core_tables_exist(chk);
    sqlite3_close(chk);

    /* Drop all */
    rc = utax_schema_drop_all(h);
    if (rc != UTAX_OK) {
        fprintf(stderr, "FAILED rc=%d err=%s\n", (int)rc, utax_db_last_error(h));
    }
    assert(rc == UTAX_OK);

    chk = sqlite_open_check(db_path);
    expect_core_tables_absent(chk);
    sqlite3_close(chk);

    /* Recreate */
    rc = utax_schema_recreate_from_file(h, schema_path);
    assert(rc == UTAX_OK);

    chk = sqlite_open_check(db_path);
    expect_core_tables_exist(chk);
    sqlite3_close(chk);

    rc = utax_db_close(h);
    assert(rc == UTAX_OK);

    remove(db_path);

    printf("All ultimateTax schema tests passed.\n");
    return 0;
}
