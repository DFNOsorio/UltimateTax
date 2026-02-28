#include "utax_db.h"
#include "utax_schema.h"
#include "utax_trades.h"
#include "utax_fifo_snapshot.h"
#include "utax_market_data.h"

#include "mock_trades.h"
#include "mock_fifo_snapshot.h"

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
    snprintf(out, out_sz, "/tmp/utax_fifo_snapshot_test_%ld.db", (long)getpid());
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

static int snapshot_matches_filter(const utax_fifo_snapshot_row *r, const utax_fifo_snapshot_filter *f) {
    if (!f) return 1;
    if (f->has_year) {
        if (f->year_mode == UTAX_YEAR_UP_TO) { if (!(r->tax_year <= f->year)) return 0; }
        else { if (!(r->tax_year == f->year)) return 0; }
    }
    if (f->has_broker && strcmp(r->broker, f->broker) != 0) return 0;
    if (f->has_ticker && strcmp(r->ticker, f->ticker) != 0) return 0;
    if (f->has_country && strcmp(r->country, f->country) != 0) return 0;
    return 1;
}

static size_t expected_total(const utax_fifo_snapshot_row *arr, size_t n, const utax_fifo_snapshot_filter *f) {
    size_t c = 0;
    for (size_t i = 0; i < n; ++i) if (snapshot_matches_filter(&arr[i], f)) c++;
    return c;
}

static size_t expected_page(size_t total, const utax_fifo_snapshot_filter *f) {
    if (!f) return total;
    int offset = f->has_offset ? f->offset : 0;
    int limit  = f->has_limit ? f->limit : -1;

    if (offset < 0) offset = 0;
    if ((size_t)offset >= total) return 0;

    size_t rem = total - (size_t)offset;
    if (limit < 0) return rem;
    return rem < (size_t)limit ? rem : (size_t)limit;
}

utax_rc utax_market_data_lookup_yahoo_date(
    const char *ticker,
    const char *date_yyyy_mm_dd,
    int include_dividend_yield,
    utax_market_quote *out_quote
) {
    (void)include_dividend_yield;

    if (!ticker || !date_yyyy_mm_dd || !out_quote) return UTAX_ERR_INVALID_ARG;
    if (strcmp(date_yyyy_mm_dd, "2026-02-10") != 0) return UTAX_ERR_NOT_FOUND;

    memset(out_quote, 0, sizeof(*out_quote));
    UTAX_STRNCPY(out_quote->ticker, sizeof(out_quote->ticker), ticker);
    UTAX_STRNCPY(out_quote->date_yyyy_mm_dd, sizeof(out_quote->date_yyyy_mm_dd), date_yyyy_mm_dd);

    if (strcmp(ticker, "AAPL") == 0) {
        UTAX_STRNCPY(out_quote->currency, sizeof(out_quote->currency), "USD");
        out_quote->conversion_rate_eur = 1.0;
        out_quote->has_conversion_rate_eur = 1;
        out_quote->close_price = 201.25;
        return UTAX_OK;
    }
    if (strcmp(ticker, "MSFT") == 0) {
        UTAX_STRNCPY(out_quote->currency, sizeof(out_quote->currency), "USD");
        out_quote->conversion_rate_eur = 1.0;
        out_quote->has_conversion_rate_eur = 1;
        out_quote->close_price = 410.75;
        return UTAX_OK;
    }
    if (strcmp(ticker, "NOMKT") == 0) {
        return UTAX_ERR_NOT_FOUND;
    }

    return UTAX_ERR_NOT_FOUND;
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

    /* fifo_snapshot count_total initially */
    long long ct = -1;
    rc = utax_fifo_snapshot_count_total(db, &ct);
    assert(rc == UTAX_OK);
    assert(ct == 0);

    /* Prepare fifo snapshots (map acq_trade_id by datetime key) */
    const size_t N = utax_mock_fifo_snapshots_count();
    assert(N <= 32);

    utax_fifo_snapshot_row inserted[32];
    memset(inserted, 0, sizeof(inserted));

    for (size_t i = 0; i < N; ++i) {
        inserted[i] = UTAX_MOCK_FIFO_SNAPSHOTS[i].row;

        long long trade_id = find_trade_id_by_datetime(db, UTAX_MOCK_FIFO_SNAPSHOTS[i].acq_trade_datetime_key);
        assert(trade_id > 0);
        inserted[i].acq_trade_id = trade_id;
    }

    /* Batch insert */
    size_t inserted_n = 0;
    rc = utax_fifo_snapshot_insert_many(db, inserted, N, &inserted_n);
    if (rc != UTAX_OK) fprintf(stderr, "batch insert failed: %s\n", utax_db_last_error(db));
    assert(rc == UTAX_OK);
    assert(inserted_n == N);

    long long lot_ids[32] = {0};
    for (size_t i = 0; i < N; ++i) {
        assert(inserted[i].lot_id > 0);
        lot_ids[i] = inserted[i].lot_id;
    }

    /* count_total after insert */
    rc = utax_fifo_snapshot_count_total(db, &ct);
    assert(rc == UTAX_OK);
    assert((size_t)ct == N);

    /* Batch failure should rollback (bad FK) */
    {
        utax_fifo_snapshot_row bad[32];
        memcpy(bad, inserted, sizeof(bad));

        /* corrupt the last row FK */
        bad[N - 1].lot_id = 0;
        bad[N - 1].acq_trade_id = 999999999LL;

        size_t done = 0;
        utax_rc rc2 = utax_fifo_snapshot_insert_many(db, bad, N, &done);
        assert(rc2 != UTAX_OK);

        /* Since we rollback on failure, count_total remains unchanged */
        long long after = -1;
        rc = utax_fifo_snapshot_count_total(db, &after);
        assert(rc == UTAX_OK);
        assert(after == ct);
        /* done indicates how many rows were processed before failure (rolled back) */
        assert(done <= N);
    }

    /* count_filtered cases */
    {
        utax_fifo_snapshot_filter f;
        memset(&f, 0, sizeof(f));
        f.has_broker = 1; UTAX_STRNCPY(f.broker, sizeof(f.broker), "IKBR");

        long long c = -1;
        rc = utax_fifo_snapshot_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);
        assert((size_t)c == expected_total(inserted, N, &f));
    }
    {
        utax_fifo_snapshot_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1; f.year = 2025; f.year_mode = UTAX_YEAR_EXACT;

        long long c = -1;
        rc = utax_fifo_snapshot_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);
        assert((size_t)c == expected_total(inserted, N, &f));
    }
    {
        utax_fifo_snapshot_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1; f.year = 2025; f.year_mode = UTAX_YEAR_UP_TO;

        long long c = -1;
        rc = utax_fifo_snapshot_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);
        assert((size_t)c == expected_total(inserted, N, &f));
    }

    /* get_filtered all */
    utax_fifo_snapshot_row rows[32];
    size_t out_n = 0, req = 0;
    {
        utax_fifo_snapshot_filter f;
        memset(&f, 0, sizeof(f));

        rc = utax_fifo_snapshot_get_filtered(db, &f, rows, 32, &out_n, &req);
        if (rc != UTAX_OK) fprintf(stderr, "get_filtered(all) failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);
        assert(out_n == N);
        assert(req == N);

        for (size_t i = 1; i < out_n; ++i) {
            assert(strcmp(rows[i-1].acq_datetime, rows[i].acq_datetime) <= 0);
        }
        for (size_t i = 0; i < out_n; ++i) {
            assert(rows[i].last_updated_stock_price >= 0.0);
            assert(rows[i].last_updated_stock_conversion_rate_eur > 0.0);
            assert(rows[i].current_lot_value_eur ==
                   rows[i].qty_remaining * (rows[i].last_updated_stock_price / rows[i].last_updated_stock_conversion_rate_eur));
            assert(rows[i].last_price_update_date[0] != '\0');
        }
    }

    /* capacity too small */
    {
        utax_fifo_snapshot_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1; f.year = 2026; f.year_mode = UTAX_YEAR_UP_TO;

        utax_fifo_snapshot_row small_rows[1];
        size_t oc = 0, rq = 0;

        rc = utax_fifo_snapshot_get_filtered(db, &f, small_rows, 1, &oc, &rq);
        assert(rc == UTAX_ERR_NO_SPACE);
        assert(oc == 0);
        assert(rq > 1);
    }

    /* pagination */
    {
        utax_fifo_snapshot_filter f;
        memset(&f, 0, sizeof(f));
        f.has_limit = 1; f.limit = 2;
        f.has_offset = 1; f.offset = 1;

        utax_fifo_snapshot_row page[2];
        size_t oc = 0, rq = 0;

        rc = utax_fifo_snapshot_get_filtered(db, &f, page, 2, &oc, &rq);
        if (rc != UTAX_OK) fprintf(stderr, "get_filtered(page) failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);

        size_t total = expected_total(inserted, N, NULL);
        size_t exp_page = expected_page(total, &f);
        assert(rq == exp_page);
        assert(oc == exp_page);

        /* compare to slice of full result */
        utax_fifo_snapshot_filter all;
        memset(&all, 0, sizeof(all));
        utax_fifo_snapshot_row full[32];
        size_t fn = 0, fr = 0;
        rc = utax_fifo_snapshot_get_filtered(db, &all, full, 32, &fn, &fr);
        assert(rc == UTAX_OK);

        for (size_t i = 0; i < oc; ++i) {
            assert(page[i].lot_id == full[i + 1].lot_id);
            assert(strcmp(page[i].acq_datetime, full[i + 1].acq_datetime) == 0);
        }
    }

    /* update_by_id + verify */
    {
        utax_fifo_snapshot_row upd = inserted[0];
        upd.qty_remaining = 9.0;
        upd.cost_per_share_eur = 88.0;
        UTAX_STRNCPY(upd.last_price_update_date, sizeof(upd.last_price_update_date), "2026-01-01 16:00");
        upd.last_updated_stock_price = 111.25;
        UTAX_STRNCPY(upd.last_updated_stock_currency, sizeof(upd.last_updated_stock_currency), "USD");
        upd.last_updated_stock_conversion_rate_eur = 1.0;

        rc = utax_fifo_snapshot_update_by_id(db, lot_ids[0], &upd);
        if (rc != UTAX_OK) fprintf(stderr, "update failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);

        utax_fifo_snapshot_filter all;
        memset(&all, 0, sizeof(all));
        size_t fn = 0, fr = 0;
        rc = utax_fifo_snapshot_get_filtered(db, &all, rows, 32, &fn, &fr);
        assert(rc == UTAX_OK);

        int found = 0;
        for (size_t i = 0; i < fn; ++i) {
            if (rows[i].lot_id == lot_ids[0]) {
                found = 1;
                assert(rows[i].qty_remaining == 9.0);
                assert(rows[i].cost_per_share_eur == 88.0);
                assert(strcmp(rows[i].last_price_update_date, "2026-01-01 16:00") == 0);
                assert(rows[i].last_updated_stock_price == 111.25);
                assert(strcmp(rows[i].last_updated_stock_currency, "USD") == 0);
                assert(rows[i].last_updated_stock_conversion_rate_eur == 1.0);
                assert(rows[i].current_lot_value_eur ==
                       rows[i].qty_remaining * (rows[i].last_updated_stock_price / rows[i].last_updated_stock_conversion_rate_eur));
            }
        }
        assert(found);
    }

    /* update one lot using market data by lot_id */
    {
        rc = utax_fifo_snapshot_update_price_by_lot_id(db, lot_ids[2], "2026-02-10");
        if (rc != UTAX_OK) fprintf(stderr, "update_price_by_lot_id failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);

        utax_fifo_snapshot_filter f;
        memset(&f, 0, sizeof(f));
        size_t fn = 0, fr = 0;
        rc = utax_fifo_snapshot_get_filtered(db, &f, rows, 32, &fn, &fr);
        assert(rc == UTAX_OK);

        int found = 0;
        for (size_t i = 0; i < fn; ++i) {
            if (rows[i].lot_id == lot_ids[2]) {
                found = 1;
                assert(strcmp(rows[i].last_price_update_date, "2026-02-10") == 0);
                assert(rows[i].last_updated_stock_price == 410.75);
                assert(strcmp(rows[i].last_updated_stock_currency, "USD") == 0);
                assert(rows[i].last_updated_stock_conversion_rate_eur == 1.0);
                assert(rows[i].current_lot_value_eur == rows[i].qty_remaining * (410.75 / 1.0));
            }
        }
        assert(found);
    }

    /* update_price_by_lot_id: not found */
    {
        rc = utax_fifo_snapshot_update_price_by_lot_id(db, 999999999LL, "2026-02-10");
        assert(rc == UTAX_ERR_NOT_FOUND);
    }

    /* add one lot with unavailable market data ticker */
    long long nomkt_lot_id = 0;
    {
        utax_fifo_snapshot_row nomkt = inserted[1];
        UTAX_STRNCPY(nomkt.ticker, sizeof(nomkt.ticker), "NOMKT");
        UTAX_STRNCPY(nomkt.acq_datetime, sizeof(nomkt.acq_datetime), "2025-02-07 11:00");
        nomkt.acq_trade_id = inserted[1].acq_trade_id;
        UTAX_STRNCPY(nomkt.last_price_update_date, sizeof(nomkt.last_price_update_date), "2025-02-07 16:00");
        nomkt.last_updated_stock_price = 10.0;
        UTAX_STRNCPY(nomkt.last_updated_stock_currency, sizeof(nomkt.last_updated_stock_currency), "USD");
        nomkt.last_updated_stock_conversion_rate_eur = 1.0;

        rc = utax_fifo_snapshot_insert(db, &nomkt, &nomkt_lot_id);
        if (rc != UTAX_OK) fprintf(stderr, "insert NOMKT snapshot failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);
        assert(nomkt_lot_id > 0);

        rc = utax_fifo_snapshot_update_price_by_lot_id(db, nomkt_lot_id, "2026-02-10");
        assert(rc == UTAX_ERR_NOT_FOUND);
    }

    /* update all lots for ticker */
    {
        size_t updated = 0;
        size_t unavailable = 0;

        rc = utax_fifo_snapshot_update_prices_by_ticker(db, "AAPL", "2026-02-10", &updated, &unavailable);
        if (rc != UTAX_OK) fprintf(stderr, "update_prices_by_ticker(AAPL) failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);
        assert(updated == 2);
        assert(unavailable == 0);

        utax_fifo_snapshot_filter f;
        memset(&f, 0, sizeof(f));
        f.has_ticker = 1;
        UTAX_STRNCPY(f.ticker, sizeof(f.ticker), "AAPL");

        size_t fn = 0, fr = 0;
        rc = utax_fifo_snapshot_get_filtered(db, &f, rows, 32, &fn, &fr);
        assert(rc == UTAX_OK);
        assert(fn == 2);

        for (size_t i = 0; i < fn; ++i) {
            assert(strcmp(rows[i].last_price_update_date, "2026-02-10") == 0);
            assert(rows[i].last_updated_stock_price == 201.25);
            assert(strcmp(rows[i].last_updated_stock_currency, "USD") == 0);
            assert(rows[i].last_updated_stock_conversion_rate_eur == 1.0);
        }
    }

    /* update_prices_by_ticker: unavailable market data handled gracefully */
    {
        size_t updated = 0;
        size_t unavailable = 0;

        rc = utax_fifo_snapshot_update_prices_by_ticker(db, "NOMKT", "2026-02-10", &updated, &unavailable);
        assert(rc == UTAX_OK);
        assert(updated == 0);
        assert(unavailable == 1);
    }

    /* paged mass update */
    {
        utax_fifo_snapshot_filter f;
        memset(&f, 0, sizeof(f));
        f.has_limit = 1;
        f.limit = 10;
        f.has_offset = 1;
        f.offset = 0;

        size_t processed = 0;
        size_t updated = 0;
        size_t unavailable = 0;

        rc = utax_fifo_snapshot_update_prices_paged(db, "2026-02-10", &f, &processed, &updated, &unavailable);
        if (rc != UTAX_OK) fprintf(stderr, "update_prices_paged failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);
        assert(processed == 4);
        assert(updated == 3);
        assert(unavailable == 1);
    }

    /* update non-existent */
    {
        utax_fifo_snapshot_row dummy = inserted[0];
        rc = utax_fifo_snapshot_update_by_id(db, 999999999LL, &dummy);
        assert(rc == UTAX_ERR_NOT_FOUND);
    }

    /* delete_by_id + verify */
    {
        long long before = 0, after = 0;
        rc = utax_fifo_snapshot_count_total(db, &before);
        assert(rc == UTAX_OK);

        rc = utax_fifo_snapshot_delete_by_id(db, lot_ids[0]);
        if (rc != UTAX_OK) fprintf(stderr, "delete failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);

        rc = utax_fifo_snapshot_count_total(db, &after);
        assert(rc == UTAX_OK);
        assert(after == before - 1);

        rc = utax_fifo_snapshot_delete_by_id(db, lot_ids[0]);
        assert(rc == UTAX_ERR_NOT_FOUND);
    }

    rc = utax_db_close(db);
    assert(rc == UTAX_OK);

    remove(db_path);

    printf("All ultimateTax fifo_snapshot tests passed.\n");
    return 0;
}
