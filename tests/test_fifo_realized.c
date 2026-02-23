#include "utax_db.h"
#include "utax_schema.h"
#include "utax_trades.h"
#include "utax_fifo_realized.h"

#include "mock_trades.h"
#include "mock_fifo_realized.h"

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
    snprintf(out, out_sz, "/tmp/utax_fifo_realized_test_%ld.db", (long)getpid());
#endif
}

static long long find_trade_id_by_datetime(utax_db_t *db, const char *dt) {
    utax_trades_filter f;
    memset(&f, 0, sizeof(f));

    utax_trades_row rows[64];
    size_t out_n = 0, req = 0;

    utax_rc rc = utax_trades_get_filtered(db, &f, rows, 64, &out_n, &req);
    assert(rc == UTAX_OK);

    for (size_t i = 0; i < out_n; ++i) {
        if (strcmp(rows[i].trade_datetime, dt) == 0) return rows[i].id;
    }
    return 0;
}

static int realized_matches_filter(const utax_fifo_realized_row *r, const utax_fifo_realized_filter *f) {
    if (!f) return 1;
    if (f->has_year) {
        if (f->year_mode == UTAX_YEAR_UP_TO) { if (!(r->tax_year <= f->year)) return 0; }
        else { if (!(r->tax_year == f->year)) return 0; }
    }
    if (f->has_broker && strcmp(r->broker, f->broker) != 0) return 0;
    if (f->has_ticker && strcmp(r->ticker, f->ticker) != 0) return 0;
    if (f->has_country && strcmp(r->country, f->country) != 0) return 0;
    if (f->has_sell_trade_id && r->sell_trade_id != f->sell_trade_id) return 0;
    if (f->has_buy_trade_id  && r->buy_trade_id  != f->buy_trade_id)  return 0;
    return 1;
}

static size_t expected_total(const utax_fifo_realized_row *arr, size_t n, const utax_fifo_realized_filter *f) {
    size_t c = 0;
    for (size_t i = 0; i < n; ++i) if (realized_matches_filter(&arr[i], f)) c++;
    return c;
}

static size_t expected_page(size_t total, const utax_fifo_realized_filter *f) {
    if (!f) return total;
    int offset = f->has_offset ? f->offset : 0;
    int limit  = f->has_limit ? f->limit : -1;

    if (offset < 0) offset = 0;
    if ((size_t)offset >= total) return 0;

    size_t rem = total - (size_t)offset;
    if (limit < 0) return rem;
    return rem < (size_t)limit ? rem : (size_t)limit;
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
    if (rc != UTAX_OK) fprintf(stderr, "schema_apply failed: %s\n", utax_db_last_error(db));
    assert(rc == UTAX_OK);

    /* Insert trades needed for FK */
    {
        const size_t tn = utax_mock_trades_count();
        for (size_t i = 0; i < tn; ++i) {
            long long id = 0;
            rc = utax_trades_insert(db, &UTAX_MOCK_TRADES[i], &id);
            if (rc != UTAX_OK) fprintf(stderr, "trade insert failed: %s\n", utax_db_last_error(db));
            assert(rc == UTAX_OK);
            assert(id > 0);
        }
    }

    long long ct = -1;
    rc = utax_fifo_realized_count_total(db, &ct);
    assert(rc == UTAX_OK);
    assert(ct == 0);

    /* Prepare realized rows (map trade ids) */
    const size_t N = utax_mock_fifo_realized_count();
    assert(N <= 32);

    utax_fifo_realized_row inserted[32];
    memset(inserted, 0, sizeof(inserted));

    for (size_t i = 0; i < N; ++i) {
        inserted[i] = UTAX_MOCK_FIFO_REALIZED[i].row;

        long long sell_id = find_trade_id_by_datetime(db, UTAX_MOCK_FIFO_REALIZED[i].sell_trade_datetime_key);
        long long buy_id  = find_trade_id_by_datetime(db, UTAX_MOCK_FIFO_REALIZED[i].buy_trade_datetime_key);
        assert(sell_id > 0 && buy_id > 0);

        inserted[i].sell_trade_id = sell_id;
        inserted[i].buy_trade_id  = buy_id;
    }

    /* Batch insert */
    size_t inserted_n = 0;
    rc = utax_fifo_realized_insert_many(db, inserted, N, &inserted_n);
    if (rc != UTAX_OK) fprintf(stderr, "batch insert failed: %s\n", utax_db_last_error(db));
    assert(rc == UTAX_OK);
    assert(inserted_n == N);

    long long realized_ids[32] = {0};
    for (size_t i = 0; i < N; ++i) {
        assert(inserted[i].realized_id > 0);
        realized_ids[i] = inserted[i].realized_id;
    }

    rc = utax_fifo_realized_count_total(db, &ct);
    assert(rc == UTAX_OK);
    assert((size_t)ct == N);

    /* Batch failure should rollback (UNIQUE(sell_trade_id, match_seq) conflict within batch) */
    {
        utax_fifo_realized_row bad[2];
        memset(bad, 0, sizeof(bad));

        /* First row should be valid and unique (match_seq 100) */
        bad[0] = inserted[0];
        bad[0].realized_id = 0;
        bad[0].match_seq = 100;

        /* Second row duplicates same (sell_trade_id, match_seq) => should fail */
        bad[1] = inserted[1];
        bad[1].realized_id = 0;
        bad[1].sell_trade_id = bad[0].sell_trade_id;
        bad[1].match_seq = 100;
        bad[1].buy_trade_id = inserted[1].buy_trade_id; /* keep FK valid */

        size_t done = 0;
        utax_rc rc2 = utax_fifo_realized_insert_many(db, bad, 2, &done);
        assert(rc2 != UTAX_OK);
        assert(done == 1 || done == 0); /* if SQLite fails at step 2, done==1 */

        long long after = -1;
        rc = utax_fifo_realized_count_total(db, &after);
        assert(rc == UTAX_OK);
        assert(after == (long long)N); /* rollback => no new rows */
    }

    /* count_filtered */
    {
        utax_fifo_realized_filter f;
        memset(&f, 0, sizeof(f));
        f.has_broker = 1; UTAX_STRNCPY(f.broker, sizeof(f.broker), "IKBR");

        long long c = -1;
        rc = utax_fifo_realized_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);
        assert((size_t)c == expected_total(inserted, N, &f));
    }

    /* get_filtered all */
    utax_fifo_realized_row rows[32];
    size_t out_n = 0, req = 0;
    {
        utax_fifo_realized_filter f;
        memset(&f, 0, sizeof(f));

        rc = utax_fifo_realized_get_filtered(db, &f, rows, 32, &out_n, &req);
        if (rc != UTAX_OK) fprintf(stderr, "get_filtered(all) failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);
        assert(out_n == N);
        assert(req == N);

        for (size_t i = 1; i < out_n; ++i) {
            assert(strcmp(rows[i-1].sell_datetime, rows[i].sell_datetime) <= 0);
        }
    }

    /* capacity too small */
    {
        utax_fifo_realized_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1; f.year = 2026; f.year_mode = UTAX_YEAR_UP_TO;

        utax_fifo_realized_row small_rows[1];
        size_t oc = 0, rq = 0;

        rc = utax_fifo_realized_get_filtered(db, &f, small_rows, 1, &oc, &rq);
        assert(rc == UTAX_ERR_NO_SPACE);
        assert(oc == 0);
        assert(rq > 1);
    }

    /* pagination */
    {
        utax_fifo_realized_filter f;
        memset(&f, 0, sizeof(f));
        f.has_limit = 1; f.limit = 2;
        f.has_offset = 1; f.offset = 1;

        utax_fifo_realized_row page[2];
        size_t oc = 0, rq = 0;

        rc = utax_fifo_realized_get_filtered(db, &f, page, 2, &oc, &rq);
        assert(rc == UTAX_OK);

        size_t total = expected_total(inserted, N, NULL);
        size_t exp_page = expected_page(total, &f);
        assert(rq == exp_page);
        assert(oc == exp_page);

        utax_fifo_realized_filter all;
        memset(&all, 0, sizeof(all));
        utax_fifo_realized_row full[32];
        size_t fn = 0, fr = 0;
        rc = utax_fifo_realized_get_filtered(db, &all, full, 32, &fn, &fr);
        assert(rc == UTAX_OK);

        for (size_t i = 0; i < oc; ++i) {
            assert(page[i].realized_id == full[i + 1].realized_id);
        }
    }

    /* update_by_id + verify */
    {
        utax_fifo_realized_row upd = inserted[0];
        upd.qty_matched = 9.0;
        upd.costs_eur = 9.9;

        rc = utax_fifo_realized_update_by_id(db, realized_ids[0], &upd);
        if (rc != UTAX_OK) fprintf(stderr, "update failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);

        utax_fifo_realized_filter all;
        memset(&all, 0, sizeof(all));
        size_t fn = 0, fr = 0;
        rc = utax_fifo_realized_get_filtered(db, &all, rows, 32, &fn, &fr);
        assert(rc == UTAX_OK);

        int found = 0;
        for (size_t i = 0; i < fn; ++i) {
            if (rows[i].realized_id == realized_ids[0]) {
                found = 1;
                assert(rows[i].qty_matched == 9.0);
                assert(rows[i].costs_eur == 9.9);
            }
        }
        assert(found);
    }

    /* update non-existent */
    {
        utax_fifo_realized_row dummy = inserted[0];
        rc = utax_fifo_realized_update_by_id(db, 999999999LL, &dummy);
        assert(rc == UTAX_ERR_NOT_FOUND);
    }

    /* delete_by_id + verify */
    {
        long long before = 0, after = 0;
        rc = utax_fifo_realized_count_total(db, &before);
        assert(rc == UTAX_OK);

        rc = utax_fifo_realized_delete_by_id(db, realized_ids[0]);
        if (rc != UTAX_OK) fprintf(stderr, "delete failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);

        rc = utax_fifo_realized_count_total(db, &after);
        assert(rc == UTAX_OK);
        assert(after == before - 1);

        rc = utax_fifo_realized_delete_by_id(db, realized_ids[0]);
        assert(rc == UTAX_ERR_NOT_FOUND);
    }

    rc = utax_db_close(db);
    assert(rc == UTAX_OK);

    remove(db_path);

    printf("All ultimateTax fifo_realized tests passed.\n");
    return 0;
}
