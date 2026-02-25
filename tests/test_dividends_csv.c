#include "utax_db.h"
#include "utax_schema.h"
#include "utax_dividends.h"

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

    if (!path || !text) {
        fprintf(stderr, "write_text_file: invalid args\n");
        assert(0);
        return;
    }

    if (!UTAX_FOPEN(f, path, "wb")) {
        fprintf(stderr, "write_text_file: failed to open '%s'\n", path);
        assert(0);
        return;
    }

    size_t n = strlen(text);
    size_t w = fwrite(text, 1, n, f);
    if (w != n) {
        fprintf(stderr, "write_text_file: fwrite failed (wrote %zu/%zu)\n", w, n);
        fclose(f);
        assert(0);
        return;
    }

    fclose(f);
}

static int year_from_dt(const char *dt16) {
    char y[5] = { dt16[0], dt16[1], dt16[2], dt16[3], 0 };
    return atoi(y);
}

int main(int argc, char **argv) {
    assert(argc >= 2);
    const char *schema_path = argv[1];

    /* temp DB */
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

    /* temp CSV #1 */
    char csv1[512];
    make_temp_path(csv1, sizeof(csv1), "csv");

    const char *txt1 =
        "TIME,BROKER,TICKER,NUMBER_OF_SHARES,PER_SHARE,AMOUNT,TAX,TAX_RATE,COUNTRY,CURRENCY,CONVERSION_RATE_1EUR\n"
        "2019-12-19,REVO,GM,3,0.38,1.14,0.17,15,US,USD,1.1117\n";
    write_text_file(csv1, txt1);

    /* parse -> dynamic array */
    utax_dividends_row *rows = NULL;
    size_t total = 0;

    rc = utax_dividends_parse_csv_file(csv1, &rows, &total);
    assert(rc == UTAX_OK);
    assert(total == 1);
    assert(rows != NULL);

    /* validate parsed row */
    assert(strcmp(rows[0].broker, "REVO") == 0);
    assert(strcmp(rows[0].ticker, "GM") == 0);
    assert(strcmp(rows[0].country, "US") == 0);
    assert(strcmp(rows[0].currency, "USD") == 0);
    assert(strcmp(rows[0].dividend_dt, "2019-12-19 00:00") == 0);
    assert(rows[0].per_share == 0.38);
    assert(rows[0].total_amount == 1.14);
    assert(rows[0].tax == 0.17);
    assert(rows[0].conversion_rate_eur > 1.0);

    /* insert array (batch) */
    size_t inserted = 0;
    rc = utax_dividends_insert_many_array(db, rows, total, &inserted);
    if (rc != UTAX_OK) fprintf(stderr, "insert_many_array failed: %s\n", utax_db_last_error(db));
    assert(rc == UTAX_OK);
    assert(inserted == 1);
    assert(rows[0].dividend_id > 0);

    /* verify DB */
    long long ctot = -1;
    rc = utax_dividends_count_total(db, &ctot);
    assert(rc == UTAX_OK);
    assert(ctot == 1);

    utax_dividends_filter f;
    memset(&f, 0, sizeof(f));
    f.has_year = 1;
    f.year = 2019;
    f.year_mode = UTAX_YEAR_EXACT;

    utax_dividends_row out[8];
    size_t out_n = 0, req = 0;
    rc = utax_dividends_get_filtered(db, &f, out, 8, &out_n, &req);
    assert(rc == UTAX_OK);
    assert(out_n == 1);
    assert(out[0].dividend_year == year_from_dt(out[0].dividend_dt));

    /* free rows */
    utax_dividends_free_rows(&rows, &total);
    assert(rows == NULL);
    assert(total == 0);

    /* temp CSV #2 (two rows) */
    char csv2[512];
    make_temp_path(csv2, sizeof(csv2), "csv");

    const char *txt2 =
        "TIME,BROKER,TICKER,NUMBER_OF_SHARES,PER_SHARE,AMOUNT,TAX,TAX_RATE,COUNTRY,CURRENCY,CONVERSION_RATE_1EUR\n"
        "2020-01-01,REVO,AAPL,1,0.50,0.50,0.05,10,US,USD,1.10\n"
        "2020-02-01,REVO,MSFT,2,0.25,0.50,0.05,10,US,USD,1.10\n";
    write_text_file(csv2, txt2);

    /* parse+insert+free in one call */
    size_t inserted2 = 0;
    rc = utax_dividends_insert_many_from_csv_file(db, csv2, &inserted2);
    if (rc != UTAX_OK) fprintf(stderr, "insert_many_from_csv_file failed: %s\n", utax_db_last_error(db));
    assert(rc == UTAX_OK);
    assert(inserted2 == 2);

    rc = utax_dividends_count_total(db, &ctot);
    assert(rc == UTAX_OK);
    assert(ctot == 3);

    /* cleanup */
    rc = utax_db_close(db);
    assert(rc == UTAX_OK);

    remove(db_path);
    remove(csv1);
    remove(csv2);

    printf("All ultimateTax dividends CSV/array tests passed.\n");
    return 0;
}
