#include "utax_db.h"
#include "utax_schema.h"
#include "utax_trades.h"

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

    /* CSV #1: two rows */
    char csv1[512];
    make_temp_path(csv1, sizeof(csv1), "csv");

    const char *txt1 =
        "DATE,TIME,BROKER,TYPE,TICKER,QTD,PER_SHARE,COMMISSION,COUNTRY,CURRENCY,CONVERSION_RATE_1EUR\n"
        "2019-09-24,14:30,REVO,BUY,OSTK,1,11.77,0,US,USD,1.1003\n"
        "2019-09-24,14:30,REVO,BUY,GE,1,9.3395,0,US,USD,1.1003\n";
    write_text_file(csv1, txt1);

    /* parse -> list */
    utax_trades_node *head = NULL;
    size_t total = 0;

    rc = utax_trades_parse_csv_file(csv1, &head, &total);
    assert(rc == UTAX_OK);
    assert(total == 2);
    assert(head != NULL);
    assert(head->next != NULL);

    /* validate first node */
    assert(strcmp(head->row.trade_datetime, "2019-09-24 14:30") == 0);
    assert(strcmp(head->row.broker, "REVO") == 0);
    assert(strcmp(head->row.type, "BUY") == 0);
    assert(strcmp(head->row.ticker, "OSTK") == 0);
    assert(head->row.quantity == 1.0);
    assert(head->row.price_per_share == 11.77);
    assert(head->row.commission == 0.0);
    assert(strcmp(head->row.country, "US") == 0);
    assert(strcmp(head->row.currency, "USD") == 0);

    /* batch insert from list */
    size_t inserted = 0;
    rc = utax_trades_insert_many_list(db, head, &inserted);
    if (rc != UTAX_OK) fprintf(stderr, "insert_many_list failed: %s\n", utax_db_last_error(db));
    assert(rc == UTAX_OK);
    assert(inserted == 2);
    assert(head->row.id > 0);
    assert(head->next->row.id > 0);

    long long ctot = -1;
    rc = utax_trades_count_total(db, &ctot);
    assert(rc == UTAX_OK);
    assert(ctot == 2);

    /* year filter should match 2019 */
    utax_trades_filter yf;
    memset(&yf, 0, sizeof(yf));
    yf.has_year = 1;
    yf.year = 2019;
    yf.year_mode = UTAX_YEAR_EXACT;

    long long cy = -1;
    rc = utax_trades_count_filtered(db, &yf, &cy);
    assert(rc == UTAX_OK);
    assert(cy == 2);

    /* free list */
    utax_trades_free_list(&head, &total);
    assert(head == NULL);
    assert(total == 0);

    /* CSV #2: one row; insert from file path */
    char csv2[512];
    make_temp_path(csv2, sizeof(csv2), "csv");

    const char *txt2 =
        "DATE,TIME,BROKER,TYPE,TICKER,QTD,PER_SHARE,COMMISSION,COUNTRY,CURRENCY,CONVERSION_RATE_1EUR\n"
        "2020-01-01,10:00,REVO,SELL,AAPL,2,100.00,1.25,US,USD,1.10\n";
    write_text_file(csv2, txt2);

    size_t inserted2 = 0;
    rc = utax_trades_insert_many_from_csv_file(db, csv2, &inserted2);
    if (rc != UTAX_OK) fprintf(stderr, "insert_many_from_csv_file failed: %s\n", utax_db_last_error(db));
    assert(rc == UTAX_OK);
    assert(inserted2 == 1);

    rc = utax_trades_count_total(db, &ctot);
    assert(rc == UTAX_OK);
    assert(ctot == 3);

    /* cleanup */
    rc = utax_db_close(db);
    assert(rc == UTAX_OK);

    remove(db_path);
    remove(csv1);
    remove(csv2);

    printf("All ultimateTax trades CSV/list tests passed.\n");
    return 0;
}
