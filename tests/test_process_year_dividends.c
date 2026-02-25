#include "utax_db.h"
#include "utax_dividends.h"
#include "utax_process_year.h"
#include "utax_schema.h"

#include <assert.h>
#include <math.h>
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

#define UTAX_NEAR(a, b) (fabs((a) - (b)) < 1e-9)

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
    snprintf(out, out_sz, "/tmp/utax_process_year_dividends_test_%ld.db", (long)getpid());
#endif
}

static const utax_dividends_country_total_row *find_country(
    const utax_dividends_country_total_row *rows,
    size_t n,
    const char *country
) {
    for (size_t i = 0; i < n; ++i) {
        if (strcmp(rows[i].country, country) == 0) return &rows[i];
    }
    return NULL;
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

    utax_dividends_row rows[] = {
        {
            .dividend_id = 0,
            .per_share = 1.0,
            .total_amount = 100.0,
            .tax = 15.0,
            .conversion_rate_eur = 1.0,
            .dividend_year = 0,
            ._pad0 = 0,
            .broker = "IKBR",
            .dividend_dt = "2025-01-15 12:00",
            .ticker = "AAPL",
            .country = "US",
            .currency = "USD"
        },
        {
            .dividend_id = 0,
            .per_share = 1.0,
            .total_amount = 55.0,
            .tax = 5.5,
            .conversion_rate_eur = 1.1,
            .dividend_year = 0,
            ._pad0 = 0,
            .broker = "IKBR",
            .dividend_dt = "2025-02-01 12:00",
            .ticker = "MSFT",
            .country = "US",
            .currency = "USD"
        },
        {
            .dividend_id = 0,
            .per_share = 1.0,
            .total_amount = 44.0,
            .tax = 4.4,
            .conversion_rate_eur = 1.1,
            .dividend_year = 0,
            ._pad0 = 0,
            .broker = "REVOLUT",
            .dividend_dt = "2025-03-10 12:00",
            .ticker = "RYA",
            .country = "IE",
            .currency = "EUR"
        },
        {
            .dividend_id = 0,
            .per_share = 1.0,
            .total_amount = 30.0,
            .tax = 3.0,
            .conversion_rate_eur = 1.5,
            .dividend_year = 0,
            ._pad0 = 0,
            .broker = "REVOLUT",
            .dividend_dt = "2025-04-20 12:00",
            .ticker = "VALE",
            .country = "BR",
            .currency = "BRL"
        },
        {
            .dividend_id = 0,
            .per_share = 1.0,
            .total_amount = 999.0,
            .tax = 99.0,
            .conversion_rate_eur = 1.0,
            .dividend_year = 0,
            ._pad0 = 0,
            .broker = "IKBR",
            .dividend_dt = "2024-12-20 12:00",
            .ticker = "AAPL",
            .country = "US",
            .currency = "USD"
        }
    };

    for (size_t i = 0; i < (sizeof(rows) / sizeof(rows[0])); ++i) {
        long long id = 0;
        rc = utax_dividends_insert(db, &rows[i], &id);
        assert(rc == UTAX_OK);
        assert(id > 0);
    }

    utax_dividends_country_total_row *rows_out = NULL;
    size_t total = 0;

    size_t no_export_total = 999;
    rc = process_year_dividends_country_totals(db, 2025, NULL, &no_export_total);
    assert(rc == UTAX_OK);
    assert(no_export_total == 999);

    rc = process_year_dividends_country_totals(db, 2025, &rows_out, &total);
    assert(rc == UTAX_OK);
    assert(total == 3);

    const utax_dividends_country_total_row *us = find_country(rows_out, total, "US");
    const utax_dividends_country_total_row *ie = find_country(rows_out, total, "IE");
    const utax_dividends_country_total_row *br = find_country(rows_out, total, "BR");

    assert(us && ie && br);

    assert(UTAX_NEAR(us->gross_amount_eur, 150.0));
    assert(UTAX_NEAR(us->taxes_eur, 20.0));
    assert(UTAX_NEAR(us->total_eur, 130.0));

    assert(UTAX_NEAR(ie->gross_amount_eur, 40.0));
    assert(UTAX_NEAR(ie->taxes_eur, 4.0));
    assert(UTAX_NEAR(ie->total_eur, 36.0));

    assert(UTAX_NEAR(br->gross_amount_eur, 20.0));
    assert(UTAX_NEAR(br->taxes_eur, 2.0));
    assert(UTAX_NEAR(br->total_eur, 18.0));

    process_year_free_dividends_country_total_rows(&rows_out, &total);

    rc = process_year_dividends_country_totals(db, 2024, &rows_out, &total);
    assert(rc == UTAX_OK);
    assert(total == 1);
    assert(strcmp(rows_out[0].country, "US") == 0);
    assert(UTAX_NEAR(rows_out[0].gross_amount_eur, 999.0));
    assert(UTAX_NEAR(rows_out[0].taxes_eur, 99.0));
    assert(UTAX_NEAR(rows_out[0].total_eur, 900.0));

    process_year_free_dividends_country_total_rows(&rows_out, &total);
    assert(rows_out == NULL);
    assert(total == 0);

    rc = utax_db_close(db);
    assert(rc == UTAX_OK);
    remove(db_path);

    printf("All ultimateTax process_year dividends totals tests passed.\n");
    return 0;
}
