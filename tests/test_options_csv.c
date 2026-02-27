#include "utax_db.h"
#include "utax_schema.h"
#include "utax_options.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <unistd.h>
#endif

#if defined(_MSC_VER)
  #define UTAX_STRNCPY(dst, dstsz, src) strncpy_s((dst), (dstsz), (src), _TRUNCATE)
  #define UTAX_FOPEN(out_fp, path, mode) (fopen_s(&(out_fp), (path), (mode)) == 0)
#else
  #define UTAX_STRNCPY(dst, dstsz, src)             \
    do {                                            \
      strncpy((dst), (src), (dstsz) - 1);           \
      (dst)[(dstsz) - 1] = '\0';                    \
    } while (0)
  #define UTAX_FOPEN(out_fp, path, mode) (((out_fp) = fopen((path), (mode))) != NULL)
#endif

static void make_temp_path(char *out, size_t out_sz, const char *prefix) {
#if defined(_WIN32)
    char tmpdir[MAX_PATH] = {0};
    DWORD n = GetTempPathA((DWORD)sizeof(tmpdir), tmpdir);
    assert(n > 0 && n < sizeof(tmpdir));

    char tmpfile[MAX_PATH] = {0};
    UINT u = GetTempFileNameA(tmpdir, prefix, 0, tmpfile);
    assert(u != 0);

    UTAX_STRNCPY(out, out_sz, tmpfile);
#else
    snprintf(out, out_sz, "/tmp/%s_%ld.tmp", prefix, (long)getpid());
#endif
}

static void write_text_file(const char *path, const char *text) {
    FILE *f = NULL;
    assert(path && text);
    if (!UTAX_FOPEN(f, path, "wb")) assert(0);
    size_t n = strlen(text);
    size_t w = fwrite(text, 1, n, f);
    assert(w == n);
    fclose(f);
}

int main(int argc, char **argv) {
    assert(argc >= 2);
    const char *schema_path = argv[1];

    char db_path[512];
    make_temp_path(db_path, sizeof(db_path), "udb");

    utax_db_open_opts opts = utax_db_open_opts_default();
    opts.create_if_missing = 1;
    opts.read_only = 0;
    opts.busy_timeout_ms = 50;

    utax_db_t *db = NULL;
    utax_rc rc = utax_db_open(db_path, &opts, &db);
    assert(rc == UTAX_OK);

    rc = utax_schema_apply_from_file(db, schema_path);
    assert(rc == UTAX_OK);

    char csv1[512];
    make_temp_path(csv1, sizeof(csv1), "csv");

    const char *txt1 =
        "BOUGHT_DATE,BROKER,EXPIRATION,TICKER,AMOUNT_X100,PER_CONTRACT,TAX,COUNTRY,CURRENCY,CONVERSION_RATE_1EUR\n"
        "2024-01-10,IKBR,2024-06-21,AAPL,1,2.50,0.20,US,USD,1.08\n"
        "2024-01-11,IKBR,2024-06-21,AAPL,-2,1.10,0,US,USD,1.08\n";
    write_text_file(csv1, txt1);

    utax_options_row *rows = NULL;
    size_t total = 0;
    rc = utax_options_parse_csv_file(csv1, &rows, &total);
    assert(rc == UTAX_OK);
    assert(total == 2);
    assert(rows != NULL);

    assert(strcmp(rows[0].bought_dt, "2024-01-10 00:00") == 0);
    assert(strcmp(rows[0].expiration_dt, "2024-06-21 00:00") == 0);
    assert(strcmp(rows[0].broker, "IKBR") == 0);
    assert(rows[1].amount_x100 == -2);

    size_t inserted = 0;
    rc = utax_options_insert_many_array(db, rows, total, &inserted);
    assert(rc == UTAX_OK);
    assert(inserted == 2);

    long long ctot = -1;
    rc = utax_options_count_total(db, &ctot);
    assert(rc == UTAX_OK);
    assert(ctot == 2);

    utax_options_filter f;
    memset(&f, 0, sizeof(f));
    f.has_ticker = 1;
    UTAX_STRNCPY(f.ticker, sizeof(f.ticker), "AAPL");

    utax_options_row out[4];
    size_t out_n = 0, req = 0;
    rc = utax_options_get_filtered(db, &f, out, 4, &out_n, &req);
    assert(rc == UTAX_OK);
    assert(out_n == 2);
    assert(req == 2);

    utax_options_free_rows(&rows, &total);
    assert(rows == NULL);
    assert(total == 0);

    char csv2[512];
    make_temp_path(csv2, sizeof(csv2), "csv");
    const char *txt2 =
        "BOUGHT_DATE,BROKER,EXPIRATION,TICKER,AMOUNT_X100,PER_CONTRACT,TAX,COUNTRY,CURRENCY,CONVERSION_RATE_1EUR\n"
        "2025-03-03,REVO,2025-09-19,MSFT,1,3.20,0.50,US,USD,1.10\n";
    write_text_file(csv2, txt2);

    size_t inserted2 = 0;
    rc = utax_options_insert_many_from_csv_file(db, csv2, &inserted2);
    assert(rc == UTAX_OK);
    assert(inserted2 == 1);

    rc = utax_options_count_total(db, &ctot);
    assert(rc == UTAX_OK);
    assert(ctot == 3);

    rc = utax_db_close(db);
    assert(rc == UTAX_OK);

    remove(db_path);
    remove(csv1);
    remove(csv2);

    printf("All ultimateTax options CSV/array tests passed.\n");
    return 0;
}
