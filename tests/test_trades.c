#include "utax_db.h"
#include "utax_schema.h"
#include "utax_trades.h"

#include "mock_trades.h"

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
    snprintf(out, out_sz, "/tmp/utax_trades_test_%ld.db", (long)getpid());
#endif
}

/* Normalize fields to what your INSERT/UPDATE SQL stores (defaults for empty strings). */
static const char *norm_broker(const char *s)  { return (s && s[0]) ? s : "IKBR"; }
static const char *norm_type(const char *s)    { return (s && s[0]) ? s : "BUY"; }
static const char *norm_country(const char *s) { return (s && s[0]) ? s : "US"; }
static const char *norm_ccy(const char *s)     { return (s && s[0]) ? s : "USD"; }

static int year_from_dt(const char *dt16) {
    /* dt format is "YYYY-MM-DD HH:MM" */
    char y[5] = { dt16[0], dt16[1], dt16[2], dt16[3], 0 };
    return atoi(y);
}

static int trade_matches_filter(const utax_trades_row *t, const utax_trades_filter *f) {
    if (!f) return 1;

    const char *b = norm_broker(t->broker);
    const char *ty = norm_type(t->type);

    int y = year_from_dt(t->trade_datetime);

    if (f->has_year) {
        if (f->year_mode == UTAX_YEAR_UP_TO) {
            if (!(y <= f->year)) return 0;
        } else {
            if (!(y == f->year)) return 0;
        }
    }
    if (f->has_broker && strcmp(b, f->broker) != 0) return 0;
    if (f->has_ticker && strcmp(t->ticker, f->ticker) != 0) return 0;
    if (f->has_type && strcmp(ty, f->type) != 0) return 0;
    return 1;
}

static size_t expected_count_for_filter(const utax_trades_row *arr, size_t n, const utax_trades_filter *f) {
    size_t c = 0;
    for (size_t i = 0; i < n; ++i) {
        if (trade_matches_filter(&arr[i], f)) c++;
    }
    return c;
}

static size_t expected_page_count(size_t total, const utax_trades_filter *f) {
    if (!f) return total;

    int offset = f->has_offset ? f->offset : 0;
    int limit  = f->has_limit  ? f->limit  : -1;

    if (offset < 0) offset = 0;
    if ((size_t)offset >= total) return 0;

    size_t remaining = total - (size_t)offset;
    if (limit < 0) return remaining;
    return (remaining < (size_t)limit) ? remaining : (size_t)limit;
}

static const utax_trades_row *find_mock_by_datetime(const char *dt) {
    size_t n = utax_mock_trades_count();
    for (size_t i = 0; i < n; ++i) {
        if (strcmp(UTAX_MOCK_TRADES[i].trade_datetime, dt) == 0) return &UTAX_MOCK_TRADES[i];
    }
    return NULL;
}

static const utax_trades_row *find_mock_by_id_in_results(long long id, const utax_trades_row *rows, size_t n) {
    for (size_t i = 0; i < n; ++i) if (rows[i].id == id) return &rows[i];
    return NULL;
}

static void assert_row_matches_expected(const utax_trades_row *got) {
    /* Match by datetime (unique), then validate fields with normalization. */
    const utax_trades_row *src = find_mock_by_datetime(got->trade_datetime);
    assert(src != NULL);

    assert(strcmp(got->broker, norm_broker(src->broker)) == 0);
    assert(strcmp(got->type, norm_type(src->type)) == 0);
    assert(strcmp(got->ticker, src->ticker) == 0);
    assert(strcmp(got->country, norm_country(src->country)) == 0);
    assert(strcmp(got->currency, norm_ccy(src->currency)) == 0);

    assert(got->quantity == src->quantity);
    assert(got->price_per_share == src->price_per_share);
    assert(got->commission == src->commission);

    /* year derived in DB */
    assert(got->trade_year == year_from_dt(src->trade_datetime));
}

int main(int argc, char **argv) {
    assert(argc >= 2);
    const char *schema_path = argv[1];
    assert(schema_path && schema_path[0]);

    /* Setup DB */
    char db_path[512];
    make_temp_db_path(db_path, sizeof(db_path));

    utax_db_open_opts opts = utax_db_open_opts_default();
    opts.create_if_missing = 1;
    opts.read_only = 0;
    opts.busy_timeout_ms = 50; /* keep tests fast/deterministic */

    utax_db_t *db = NULL;
    utax_rc rc = utax_db_open(db_path, &opts, &db);
    if (rc != UTAX_OK) {
        fprintf(stderr, "open failed: %s\n", utax_db_last_error(db));
    }
    assert(rc == UTAX_OK);
    assert(db != NULL);

    rc = utax_schema_apply_from_file(db, schema_path);
    if (rc != UTAX_OK) {
        fprintf(stderr, "schema_apply failed rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
    }
    assert(rc == UTAX_OK);

    /* ---- count_total initially ---- */
    long long ct = -1;
    rc = utax_trades_count_total(db, &ct);
    assert(rc == UTAX_OK);
    assert(ct == 0);

    /* ---- insert all mocks ---- */
    const size_t N = utax_mock_trades_count();
    long long ids[64] = {0};
    assert(N <= 64);

    for (size_t i = 0; i < N; ++i) {
        long long id = 0;
        rc = utax_trades_insert(db, &UTAX_MOCK_TRADES[i], &id);
        if (rc != UTAX_OK) {
            fprintf(stderr, "insert failed rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
        }
        assert(rc == UTAX_OK);
        assert(id > 0);
        ids[i] = id;
    }

    /* ---- count_total after insert ---- */
    rc = utax_trades_count_total(db, &ct);
    assert(rc == UTAX_OK);
    assert((size_t)ct == N);

    /* ---- count_filtered combinations ---- */
    {
        utax_trades_filter f;
        memset(&f, 0, sizeof(f));

        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "IKBR");

        long long c = -1;
        rc = utax_trades_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);

        size_t exp = expected_count_for_filter(UTAX_MOCK_TRADES, N, &f);
        assert((size_t)c == exp);
    }
    {
        utax_trades_filter f;
        memset(&f, 0, sizeof(f));

        f.has_year = 1;
        f.year = 2025;
        f.year_mode = UTAX_YEAR_EXACT;

        long long c = -1;
        rc = utax_trades_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);

        size_t exp = expected_count_for_filter(UTAX_MOCK_TRADES, N, &f);
        assert((size_t)c == exp);
    }
    {
        utax_trades_filter f;
        memset(&f, 0, sizeof(f));

        f.has_year = 1;
        f.year = 2025;
        f.year_mode = UTAX_YEAR_UP_TO;

        long long c = -1;
        rc = utax_trades_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);

        size_t exp = expected_count_for_filter(UTAX_MOCK_TRADES, N, &f);
        assert((size_t)c == exp);
    }
    {
        utax_trades_filter f;
        memset(&f, 0, sizeof(f));

        f.has_ticker = 1;
        UTAX_STRNCPY(f.ticker, sizeof(f.ticker), "AAPL");

        f.has_type = 1;
        UTAX_STRNCPY(f.type, sizeof(f.type), "SELL");

        long long c = -1;
        rc = utax_trades_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);

        size_t exp = expected_count_for_filter(UTAX_MOCK_TRADES, N, &f);
        assert((size_t)c == exp);
    }

    /* ---- get_filtered: read all ---- */
    utax_trades_row rows[64];
    size_t out_n = 0, req_n = 0;

    {
        utax_trades_filter f;
        memset(&f, 0, sizeof(f)); /* no filters => all */

        rc = utax_trades_get_filtered(db, &f, rows, 64, &out_n, &req_n);
        if (rc != UTAX_OK) fprintf(stderr, "get_filtered(all) rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
        assert(rc == UTAX_OK);
        assert(out_n == N);
        assert(req_n == N);

        /* verify ordering (trade_datetime ASC) and content */
        for (size_t i = 0; i < out_n; ++i) {
            if (i > 0) {
                assert(strcmp(rows[i-1].trade_datetime, rows[i].trade_datetime) <= 0);
            }
            assert_row_matches_expected(&rows[i]);
        }
    }

    /* ---- get_filtered: capacity too small ---- */
    {
        utax_trades_filter f;
        memset(&f, 0, sizeof(f));

        /* choose a filter that matches > 1 */
        f.has_year = 1;
        f.year = 2026;
        f.year_mode = UTAX_YEAR_UP_TO;

        utax_trades_row small_rows[1];
        size_t outc = 0, req = 0;

        rc = utax_trades_get_filtered(db, &f, small_rows, 1, &outc, &req);
        assert(rc == UTAX_ERR_NO_SPACE);
        assert(outc == 0);
        assert(req > 1);
    }

    /* ---- pagination ---- */
    {
        utax_trades_filter f;
        memset(&f, 0, sizeof(f));

        /* page over all trades, limit 2, offset 1 */
        f.has_limit = 1;  f.limit = 2;
        f.has_offset = 1; f.offset = 1;

        utax_trades_row page[2];
        size_t outc = 0, req = 0;

        rc = utax_trades_get_filtered(db, &f, page, 2, &outc, &req);
        if (rc != UTAX_OK) fprintf(stderr, "get_filtered(page) rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
        assert(rc == UTAX_OK);

        /* expected page size */
        size_t total = expected_count_for_filter(UTAX_MOCK_TRADES, N, NULL);
        size_t exp_page = expected_page_count(total, &f);

        assert(req == exp_page);
        assert(outc == exp_page);

        /* verify page rows match the global ordered list slice */
        utax_trades_filter all;
        memset(&all, 0, sizeof(all));
        size_t out_all = 0, req_all = 0;
        rc = utax_trades_get_filtered(db, &all, rows, 64, &out_all, &req_all);
        assert(rc == UTAX_OK);

        for (size_t i = 0; i < outc; ++i) {
            assert(strcmp(page[i].trade_datetime, rows[i + 1].trade_datetime) == 0);
            assert(page[i].id == rows[i + 1].id);
        }
    }

    /* ---- update_by_id + verify ---- */
    {
        /* Update the trade we inserted with datetime "2025-02-05 10:00" (unique). */
        long long target_id = 0;

        utax_trades_filter all;
        memset(&all, 0, sizeof(all));
        rc = utax_trades_get_filtered(db, &all, rows, 64, &out_n, &req_n);
        assert(rc == UTAX_OK);

        for (size_t i = 0; i < out_n; ++i) {
            if (strcmp(rows[i].trade_datetime, "2025-02-05 10:00") == 0) {
                target_id = rows[i].id;
                break;
            }
        }
        assert(target_id > 0);

        utax_trades_row upd = rows[0]; /* start with something, then overwrite fields */
        memset(&upd, 0, sizeof(upd));
        UTAX_STRNCPY(upd.broker, sizeof(upd.broker), "REVOLUT");
        UTAX_STRNCPY(upd.ticker, sizeof(upd.ticker), "AAPL");
        UTAX_STRNCPY(upd.trade_datetime, sizeof(upd.trade_datetime), "2025-02-05 10:00");
        UTAX_STRNCPY(upd.type, sizeof(upd.type), "BUY");
        UTAX_STRNCPY(upd.country, sizeof(upd.country), "US");
        UTAX_STRNCPY(upd.currency, sizeof(upd.currency), "USD");

        upd.quantity = 33.0;
        upd.price_per_share = 55.0;
        upd.commission = 9.0;
        upd.conversion_rate_eur = 1.07;

        rc = utax_trades_update_by_id(db, target_id, &upd);
        if (rc != UTAX_OK) fprintf(stderr, "update rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
        assert(rc == UTAX_OK);

        /* Fetch all and locate by id */
        rc = utax_trades_get_filtered(db, &all, rows, 64, &out_n, &req_n);
        assert(rc == UTAX_OK);

        const utax_trades_row *got = find_mock_by_id_in_results(target_id, rows, out_n);
        assert(got != NULL);

        assert(got->quantity == 33.0);
        assert(got->price_per_share == 55.0);
        assert(got->commission == 9.0);
        assert(got->conversion_rate_eur == 1.07);
    }

    /* ---- update non-existent id ---- */
    {
        utax_trades_row dummy;
        memset(&dummy, 0, sizeof(dummy));
        UTAX_STRNCPY(dummy.broker, sizeof(dummy.broker), "IKBR");
        UTAX_STRNCPY(dummy.ticker, sizeof(dummy.ticker), "AAPL");
        UTAX_STRNCPY(dummy.trade_datetime, sizeof(dummy.trade_datetime), "2024-01-10 09:30");
        UTAX_STRNCPY(dummy.type, sizeof(dummy.type), "BUY");
        UTAX_STRNCPY(dummy.country, sizeof(dummy.country), "US");
        UTAX_STRNCPY(dummy.currency, sizeof(dummy.currency), "USD");
        dummy.quantity = 1.0;
        dummy.price_per_share = 1.0;
        dummy.commission = 0.0;
        dummy.conversion_rate_eur = 1.0;

        rc = utax_trades_update_by_id(db, 999999999LL, &dummy);
        assert(rc == UTAX_ERR_NOT_FOUND);
    }

    /* ---- delete_by_id + verify ---- */
    {
        /* delete first inserted trade (ids[0]) */
        long long before = 0, after = 0;
        rc = utax_trades_count_total(db, &before);
        assert(rc == UTAX_OK);

        rc = utax_trades_delete_by_id(db, ids[0]);
        if (rc != UTAX_OK) fprintf(stderr, "delete rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
        assert(rc == UTAX_OK);

        rc = utax_trades_count_total(db, &after);
        assert(rc == UTAX_OK);
        assert(after == before - 1);

        /* deleting again should return not found */
        rc = utax_trades_delete_by_id(db, ids[0]);
        assert(rc == UTAX_ERR_NOT_FOUND);
    }

    rc = utax_db_close(db);
    assert(rc == UTAX_OK);

    remove(db_path);

    printf("All ultimateTax trades tests passed.\n");
    return 0;
}
