#include "utax_db.h"
#include "utax_schema.h"
#include "utax_dividends.h"
#include "mock_dividends.h"

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
    snprintf(out, out_sz, "/tmp/utax_div_test_%ld.db", (long)getpid());
#endif
}

static int year_from_dt(const char *dt16) {
    /* dt format is "YYYY-MM-DD HH:MM" */
    char y[5] = { dt16[0], dt16[1], dt16[2], dt16[3], 0 };
    return atoi(y);
}

static int matches_filter(const utax_dividends_row *d, const utax_dividends_filter *f) {
    if (!f) return 1;

    if (f->has_year) {
        int y = year_from_dt(d->dividend_dt);
        if (f->year_mode == UTAX_YEAR_UP_TO) { if (!(y <= f->year)) return 0; }
        else { if (!(y == f->year)) return 0; }
    }
    if (f->has_broker && strcmp(d->broker, f->broker) != 0) return 0;
    if (f->has_ticker && strcmp(d->ticker, f->ticker) != 0) return 0;
    if (f->has_country && strcmp(d->country, f->country) != 0) return 0;
    if (f->has_currency && strcmp(d->currency, f->currency) != 0) return 0;

    return 1;
}

static size_t expected_total(const utax_dividends_row *arr, size_t n, const utax_dividends_filter *f) {
    size_t c = 0;
    for (size_t i = 0; i < n; ++i) if (matches_filter(&arr[i], f)) c++;
    return c;
}

static size_t expected_page(size_t total, const utax_dividends_filter *f) {
    if (!f) return total;
    int offset = f->has_offset ? f->offset : 0;
    int limit  = f->has_limit ? f->limit : -1;

    if (offset < 0) offset = 0;
    if ((size_t)offset >= total) return 0;

    size_t rem = total - (size_t)offset;
    if (limit < 0) return rem;
    return rem < (size_t)limit ? rem : (size_t)limit;
}

static void assert_row_matches_expected_by_dt(const utax_dividends_row *got) {
    /* Match by dividend_dt (unique in mocks) */
    const size_t N = utax_mock_dividends_count();
    const utax_dividends_row *src = NULL;
    for (size_t i = 0; i < N; ++i) {
        if (strcmp(UTAX_MOCK_DIVIDENDS[i].dividend_dt, got->dividend_dt) == 0) {
            src = &UTAX_MOCK_DIVIDENDS[i];
            break;
        }
    }
    assert(src != NULL);

    assert(strcmp(got->broker, src->broker) == 0);
    assert(strcmp(got->ticker, src->ticker) == 0);
    assert(strcmp(got->country, src->country) == 0);
    assert(strcmp(got->currency, src->currency) == 0);

    assert(got->per_share == src->per_share);
    assert(got->total_amount == src->total_amount);
    assert(got->tax == src->tax);
    assert(got->conversion_rate_eur == src->conversion_rate_eur);

    /* DB-derived */
    assert(got->dividend_year == year_from_dt(got->dividend_dt));
}

int main(int argc, char **argv) {
    assert(argc >= 2);
    const char *schema_path = argv[1];
    assert(schema_path && schema_path[0]);

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
    if (rc != UTAX_OK) fprintf(stderr, "schema_apply failed: %s\n", utax_db_last_error(db));
    assert(rc == UTAX_OK);

    long long ctot = -1;
    rc = utax_dividends_count_total(db, &ctot);
    assert(rc == UTAX_OK);
    assert(ctot == 0);

    /* insert mocks */
    const size_t N = utax_mock_dividends_count();
    long long ids[32] = {0};
    assert(N <= 32);

    for (size_t i = 0; i < N; ++i) {
        long long id = 0;
        rc = utax_dividends_insert(db, &UTAX_MOCK_DIVIDENDS[i], &id);
        if (rc != UTAX_OK) fprintf(stderr, "insert failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);
        assert(id > 0);
        ids[i] = id;
    }

    rc = utax_dividends_count_total(db, &ctot);
    assert(rc == UTAX_OK);
    assert((size_t)ctot == N);

    /* count_filtered */
    {
        utax_dividends_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1; f.year = 2025; f.year_mode = UTAX_YEAR_EXACT;

        long long c = -1;
        rc = utax_dividends_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);
        assert((size_t)c == expected_total(UTAX_MOCK_DIVIDENDS, N, &f));
    }

    {
        utax_dividends_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1; f.year = 2025; f.year_mode = UTAX_YEAR_UP_TO;

        long long c = -1;
        rc = utax_dividends_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);
        assert((size_t)c == expected_total(UTAX_MOCK_DIVIDENDS, N, &f));
    }

    {
        utax_dividends_filter f;
        memset(&f, 0, sizeof(f));
        f.has_broker = 1; UTAX_STRNCPY(f.broker, sizeof(f.broker), "IKBR");
        f.has_ticker = 1; UTAX_STRNCPY(f.ticker, sizeof(f.ticker), "AAPL");

        long long c = -1;
        rc = utax_dividends_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);
        assert((size_t)c == expected_total(UTAX_MOCK_DIVIDENDS, N, &f));
    }

    /* get_filtered all + validate rows + ordering + dividend_year */
    {
        utax_dividends_filter f;
        memset(&f, 0, sizeof(f));

        utax_dividends_row out[32];
        size_t out_n = 0, req = 0;

        rc = utax_dividends_get_filtered(db, &f, out, 32, &out_n, &req);
        if (rc != UTAX_OK) fprintf(stderr, "get_filtered(all) failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);

        assert(out_n == N);
        assert(req == N);

        for (size_t i = 1; i < out_n; ++i) {
            assert(strcmp(out[i-1].dividend_dt, out[i].dividend_dt) <= 0);
        }

        for (size_t i = 0; i < out_n; ++i) {
            assert_row_matches_expected_by_dt(&out[i]);
        }
    }

    /* capacity too small */
    {
        utax_dividends_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1; f.year = 2026; f.year_mode = UTAX_YEAR_UP_TO;

        utax_dividends_row small_rows[1];
        size_t out_n = 0, req = 0;

        rc = utax_dividends_get_filtered(db, &f, small_rows, 1, &out_n, &req);
        assert(rc == UTAX_ERR_NO_SPACE);
        assert(out_n == 0);
        assert(req > 1);
    }

    /* pagination */
    {
        utax_dividends_filter f;
        memset(&f, 0, sizeof(f));
        f.has_limit = 1;  f.limit = 2;
        f.has_offset = 1; f.offset = 1;

        utax_dividends_row page[2];
        size_t out_n = 0, req = 0;

        rc = utax_dividends_get_filtered(db, &f, page, 2, &out_n, &req);
        if (rc != UTAX_OK) fprintf(stderr, "get_filtered(page) failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);

        size_t total = expected_total(UTAX_MOCK_DIVIDENDS, N, NULL);
        size_t exp_page = expected_page(total, &f);
        assert(req == exp_page);
        assert(out_n == exp_page);

        /* Ensure the page is a slice of the full ordered results */
        utax_dividends_filter all;
        memset(&all, 0, sizeof(all));
        utax_dividends_row full[32];
        size_t full_n = 0, full_req = 0;
        rc = utax_dividends_get_filtered(db, &all, full, 32, &full_n, &full_req);
        assert(rc == UTAX_OK);

        for (size_t i = 0; i < out_n; ++i) {
            assert(page[i].dividend_id == full[i + 1].dividend_id);
            assert(strcmp(page[i].dividend_dt, full[i + 1].dividend_dt) == 0);
        }
    }

    /* update_by_id + verify */
    {
        utax_dividends_row upd = UTAX_MOCK_DIVIDENDS[0];
        upd.per_share = 0.99;
        upd.total_amount = 99.0;
        upd.tax = 9.0;
        upd.conversion_rate_eur = 1.11;
        /* keep dt same so we can find it by dt in full read */

        rc = utax_dividends_update_by_id(db, ids[0], &upd);
        if (rc != UTAX_OK) fprintf(stderr, "update failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);

        utax_dividends_filter all;
        memset(&all, 0, sizeof(all));
        utax_dividends_row out[32];
        size_t out_n = 0, req = 0;

        rc = utax_dividends_get_filtered(db, &all, out, 32, &out_n, &req);
        assert(rc == UTAX_OK);

        int found = 0;
        for (size_t i = 0; i < out_n; ++i) {
            if (out[i].dividend_id == ids[0]) {
                found = 1;
                assert(out[i].per_share == 0.99);
                assert(out[i].total_amount == 99.0);
                assert(out[i].tax == 9.0);
                assert(out[i].conversion_rate_eur == 1.11);
                assert(out[i].dividend_year == year_from_dt(out[i].dividend_dt));
            }
        }
        assert(found);
    }

    /* update non-existent */
    {
        utax_dividends_row dummy = UTAX_MOCK_DIVIDENDS[0];
        rc = utax_dividends_update_by_id(db, 999999999LL, &dummy);
        assert(rc == UTAX_ERR_NOT_FOUND);
    }

    /* delete_by_id + verify count decreases */
    {
        long long before = 0, after = 0;
        rc = utax_dividends_count_total(db, &before);
        assert(rc == UTAX_OK);

        rc = utax_dividends_delete_by_id(db, ids[0]);
        if (rc != UTAX_OK) fprintf(stderr, "delete failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);

        rc = utax_dividends_count_total(db, &after);
        assert(rc == UTAX_OK);
        assert(after == before - 1);

        rc = utax_dividends_delete_by_id(db, ids[0]);
        assert(rc == UTAX_ERR_NOT_FOUND);
    }

    rc = utax_db_close(db);
    assert(rc == UTAX_OK);

    remove(db_path);

    printf("All ultimateTax dividends tests passed.\n");
    return 0;
}
