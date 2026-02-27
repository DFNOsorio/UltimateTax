#include "utax_db.h"
#include "utax_schema.h"
#include "utax_options.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

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

    DeleteFileA(tmpfile);
    UTAX_STRNCPY(out, out_sz, tmpfile);
#else
    snprintf(out, out_sz, "/tmp/utax_options_test_%ld.db", (long)getpid());
#endif
}

static void fill_row(utax_options_row *r,
                     const char *broker,
                     const char *bought_dt,
                     const char *exp_dt,
                     const char *ticker,
                     int amount_x100,
                     double per_contract,
                     double tax,
                     const char *country,
                     const char *currency,
                     double conv) {
    memset(r, 0, sizeof(*r));
    UTAX_STRNCPY(r->broker, sizeof(r->broker), broker);
    UTAX_STRNCPY(r->bought_dt, sizeof(r->bought_dt), bought_dt);
    UTAX_STRNCPY(r->expiration_dt, sizeof(r->expiration_dt), exp_dt);
    UTAX_STRNCPY(r->ticker, sizeof(r->ticker), ticker);
    UTAX_STRNCPY(r->country, sizeof(r->country), country);
    UTAX_STRNCPY(r->currency, sizeof(r->currency), currency);
    r->amount_x100 = amount_x100;
    r->per_contract = per_contract;
    r->tax = tax;
    r->conversion_rate_eur = conv;
}

int main(int argc, char **argv) {
    assert(argc >= 2);
    const char *schema_path = argv[1];

    char db_path[512];
    make_temp_db_path(db_path, sizeof(db_path));

    utax_db_open_opts opts = utax_db_open_opts_default();
    opts.create_if_missing = 1;
    opts.read_only = 0;
    opts.busy_timeout_ms = 50;

    utax_db_t *db = NULL;
    utax_rc rc = utax_db_open(db_path, &opts, &db);
    assert(rc == UTAX_OK);

    rc = utax_schema_apply_from_file(db, schema_path);
    assert(rc == UTAX_OK);

    utax_options_row rows[4];
    fill_row(&rows[0], "IKBR", "2024-01-10 00:00", "2024-06-21 00:00", "AAPL",  1,  2.5, 0.2, "US", "USD", 1.08);
    fill_row(&rows[1], "IKBR", "2024-01-11 00:00", "2024-06-21 00:00", "AAPL", -2,  1.1, 0.0, "US", "USD", 1.08);
    fill_row(&rows[2], "REVO", "2025-03-03 00:00", "2025-09-19 00:00", "MSFT",  1,  3.2, 0.5, "US", "USD", 1.10);
    fill_row(&rows[3], "REVO", "2025-04-01 00:00", "2025-12-20 00:00", "NVDA", -1, 10.0, 0.0, "US", "USD", 1.12);

    size_t inserted = 0;
    rc = utax_options_insert_many(db, rows, 4, &inserted);
    if (rc != UTAX_OK) fprintf(stderr, "insert_many failed: %s\n", utax_db_last_error(db));
    assert(rc == UTAX_OK);
    assert(inserted == 4);
    assert(rows[0].option_id > 0);

    long long total = -1;
    rc = utax_options_count_total(db, &total);
    assert(rc == UTAX_OK);
    assert(total == 4);

    utax_options_filter f;
    memset(&f, 0, sizeof(f));
    f.has_year = 1;
    f.year = 2024;
    f.year_mode = UTAX_YEAR_EXACT;

    long long c = -1;
    rc = utax_options_count_filtered(db, &f, &c);
    assert(rc == UTAX_OK);
    assert(c == 2);

    memset(&f, 0, sizeof(f));
    f.has_broker = 1;
    UTAX_STRNCPY(f.broker, sizeof(f.broker), "REVO");
    f.has_limit = 1;
    f.limit = 1;

    utax_options_row out[2];
    size_t out_n = 0, req = 0;
    rc = utax_options_get_filtered(db, &f, out, 2, &out_n, &req);
    assert(rc == UTAX_OK);
    assert(out_n == 1);
    assert(req == 1);
    assert(strcmp(out[0].broker, "REVO") == 0);

    utax_options_row upd = rows[0];
    upd.amount_x100 = -3;
    rc = utax_options_update_by_id(db, rows[0].option_id, &upd);
    assert(rc == UTAX_OK);

    memset(&f, 0, sizeof(f));
    f.has_ticker = 1;
    UTAX_STRNCPY(f.ticker, sizeof(f.ticker), "AAPL");
    rc = utax_options_get_filtered(db, &f, out, 2, &out_n, &req);
    assert(rc == UTAX_OK);
    assert(out_n == 2);
    assert(out[0].amount_x100 == -3 || out[1].amount_x100 == -3);

    rc = utax_options_delete_by_id(db, rows[3].option_id);
    assert(rc == UTAX_OK);

    rc = utax_options_count_total(db, &total);
    assert(rc == UTAX_OK);
    assert(total == 3);

    rc = utax_db_close(db);
    assert(rc == UTAX_OK);

    remove(db_path);

    printf("All ultimateTax options tests passed.\n");
    return 0;
}
