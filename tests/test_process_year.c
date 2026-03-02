#include "utax_corporate_actions.h"
#include "utax_db.h"
#include "utax_fifo_realized.h"
#include "utax_fifo_snapshot.h"
#include "utax_fifo_snapshot_action_applied.h"
#include "utax_process_year.h"
#include "utax_schema.h"
#include "utax_trades.h"

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
    char tmpfile[MAX_PATH] = {0};
    DWORD n = GetTempPathA((DWORD)sizeof(tmpdir), tmpdir);
    UINT u;
    assert(n > 0 && n < sizeof(tmpdir));
    u = GetTempFileNameA(tmpdir, "utx", 0, tmpfile);
    assert(u != 0);
    DeleteFileA(tmpfile);
    UTAX_STRNCPY(out, out_sz, tmpfile);
#else
    snprintf(out, out_sz, "/tmp/utax_process_year_test_%ld.db", (long)getpid());
#endif
}

static long long find_trade_id_by_dt(utax_db_t *db, const char *dt) {
    utax_trades_filter f;
    utax_trades_row rows[64];
    size_t out_n = 0, req = 0;
    memset(&f, 0, sizeof(f));
    assert(utax_trades_get_filtered(db, &f, rows, 64, &out_n, &req) == UTAX_OK);
    for (size_t i = 0; i < out_n; ++i) {
        if (strcmp(rows[i].trade_datetime, dt) == 0) return rows[i].id;
    }
    return 0;
}

int main(int argc, char **argv) {
    utax_db_t *db = NULL;
    utax_rc rc;
    char db_path[512];
    const char *schema_path;
    utax_trades_row trades[] = {
        {.id=0,.quantity=4.0,.price_per_share=90.0,.commission=1.0,.conversion_rate_eur=1.0,.trade_year=0,.broker="IKBR",.trade_datetime="2024-11-01 10:00",.type="BUY",.ticker="XYZ",.country="US",.currency="USD"},
        {.id=0,.quantity=10.0,.price_per_share=100.0,.commission=2.0,.conversion_rate_eur=1.0,.trade_year=0,.broker="IKBR",.trade_datetime="2025-01-10 10:00",.type="BUY",.ticker="XYZ",.country="US",.currency="USD"},
        {.id=0,.quantity=6.0,.price_per_share=110.0,.commission=3.0,.conversion_rate_eur=1.0,.trade_year=0,.broker="IKBR",.trade_datetime="2025-02-10 10:00",.type="BUY",.ticker="XYZ",.country="US",.currency="USD"},
        {.id=0,.quantity=12.0,.price_per_share=150.0,.commission=6.0,.conversion_rate_eur=1.0,.trade_year=0,.broker="IKBR",.trade_datetime="2025-06-10 10:00",.type="SELL",.ticker="XYZ",.country="US",.currency="USD"},
        {.id=0,.quantity=1.0,.price_per_share=160.0,.commission=1.0,.conversion_rate_eur=1.0,.trade_year=0,.broker="IKBR",.trade_datetime="2025-07-01 10:00",.type="SELL",.ticker="XYZ",.country="US",.currency="USD"},
        {.id=0,.quantity=5.0,.price_per_share=20.0,.commission=0.0,.conversion_rate_eur=1.0,.trade_year=0,.broker="REVOLUT",.trade_datetime="2025-01-15 10:00",.type="BUY",.ticker="TCS",.country="US",.currency="USD"},
        {.id=0,.quantity=2.0,.price_per_share=30.0,.commission=0.0,.conversion_rate_eur=1.0,.trade_year=0,.broker="REVOLUT",.trade_datetime="2025-08-01 10:00",.type="SELL",.ticker="TCS",.country="US",.currency="USD"}
    };
    long long seeded_lot_id = 0;
    long long split_action_id = 0;
    long long cash_action_id = 0;

    assert(argc >= 2);
    schema_path = argv[1];

    make_temp_db_path(db_path, sizeof(db_path));
    {
        utax_db_open_opts opts = utax_db_open_opts_default();
        opts.create_if_missing = 1;
        opts.read_only = 0;
        opts.busy_timeout_ms = 50;
        rc = utax_db_open(db_path, &opts, &db);
        assert(rc == UTAX_OK);
    }

    rc = utax_schema_apply_from_file(db, schema_path);
    assert(rc == UTAX_OK);

    for (size_t i = 0; i < sizeof(trades)/sizeof(trades[0]); ++i) {
        long long id = 0;
        rc = utax_trades_insert(db, &trades[i], &id);
        assert(rc == UTAX_OK && id > 0);
    }

    {
        utax_fifo_snapshot_row s;
        memset(&s, 0, sizeof(s));
        s.acq_trade_id = find_trade_id_by_dt(db, "2024-11-01 10:00");
        assert(s.acq_trade_id > 0);
        s.qty_remaining = 4.0;
        s.cost_per_share_eur = 90.0;
        s.acq_commission_eur = 1.0;
        s.tax_year = 2024;
        UTAX_STRNCPY(s.broker, sizeof(s.broker), "IKBR");
        UTAX_STRNCPY(s.ticker, sizeof(s.ticker), "XYZ");
        UTAX_STRNCPY(s.acq_datetime, sizeof(s.acq_datetime), "2024-11-01 10:00");
        UTAX_STRNCPY(s.country, sizeof(s.country), "US");
        rc = utax_fifo_snapshot_insert(db, &s, &seeded_lot_id);
        assert(rc == UTAX_OK && seeded_lot_id > 0);
    }

    {
        utax_corporate_actions_row a;
        memset(&a, 0, sizeof(a));
        a.from_qty = 1.0;
        a.to_qty = 2.0;
        UTAX_STRNCPY(a.broker, sizeof(a.broker), "IKBR");
        UTAX_STRNCPY(a.action_date, sizeof(a.action_date), "2025-03-01");
        UTAX_STRNCPY(a.action_type, sizeof(a.action_type), "SPLIT");
        UTAX_STRNCPY(a.from_ticker, sizeof(a.from_ticker), "XYZ");
        rc = utax_corporate_actions_insert(db, &a, &split_action_id);
        assert(rc == UTAX_OK && split_action_id > 0);
    }
    {
        utax_corporate_actions_row a;
        memset(&a, 0, sizeof(a));
        a.from_qty = 1.0;
        a.to_qty = 5.0;
        UTAX_STRNCPY(a.broker, sizeof(a.broker), "IKBR");
        UTAX_STRNCPY(a.action_date, sizeof(a.action_date), "2025-03-15");
        UTAX_STRNCPY(a.action_type, sizeof(a.action_type), "CASH");
        UTAX_STRNCPY(a.from_ticker, sizeof(a.from_ticker), "XYZ");
        rc = utax_corporate_actions_insert(db, &a, &cash_action_id);
        assert(rc == UTAX_OK && cash_action_id > 0);
    }

    rc = process_year_trades(db, 2025, NULL, NULL, NULL);
    assert(rc == UTAX_OK);

    {
        utax_fifo_realized_filter f;
        utax_fifo_realized_row rows[8];
        size_t out_n = 0, req = 0;
        long long cnt = 0;
        memset(&f, 0, sizeof(f));
        f.has_year = 1;
        f.year = 2025;
        f.year_mode = UTAX_YEAR_EXACT;
        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "IKBR");

        rc = utax_fifo_realized_count_filtered(db, &f, &cnt);
        assert(rc == UTAX_OK && cnt == 3);
        rc = utax_fifo_realized_get_filtered(db, &f, rows, 8, &out_n, &req);
        assert(rc == UTAX_OK && out_n == 3);

        assert(rows[0].match_seq == 1 && UTAX_NEAR(rows[0].qty_matched, 8.0));
        assert(UTAX_NEAR(rows[0].acquisition_value_eur, 360.0));
        assert(UTAX_NEAR(rows[0].sale_value_eur, 1200.0));
        assert(UTAX_NEAR(rows[0].costs_eur, 1.0));
        assert(strcmp(rows[0].buy_datetime, "2024-11-01 10:00") == 0);

        assert(rows[1].match_seq == 2 && UTAX_NEAR(rows[1].qty_matched, 4.0));
        assert(UTAX_NEAR(rows[1].acquisition_value_eur, 200.0));
        assert(UTAX_NEAR(rows[1].sale_value_eur, 600.0));
        assert(UTAX_NEAR(rows[1].costs_eur, 6.0));
        assert(strcmp(rows[1].buy_datetime, "2025-01-10 10:00") == 0);

        assert(rows[2].match_seq == 1 && UTAX_NEAR(rows[2].qty_matched, 1.0));
        assert(UTAX_NEAR(rows[2].acquisition_value_eur, 50.0));
        assert(UTAX_NEAR(rows[2].sale_value_eur, 160.0));
        assert(UTAX_NEAR(rows[2].costs_eur, 1.0));
    }

    {
        utax_fifo_realized_filter f;
        utax_fifo_realized_row rows[4];
        size_t out_n = 0, req = 0;
        long long cnt = 0;
        memset(&f, 0, sizeof(f));
        f.has_year = 1;
        f.year = 2025;
        f.year_mode = UTAX_YEAR_EXACT;
        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "REVOLUT");
        rc = utax_fifo_realized_count_filtered(db, &f, &cnt);
        assert(rc == UTAX_OK && cnt == 1);
        rc = utax_fifo_realized_get_filtered(db, &f, rows, 4, &out_n, &req);
        assert(rc == UTAX_OK && out_n == 1);
        assert(UTAX_NEAR(rows[0].qty_matched, 2.0));
        assert(UTAX_NEAR(rows[0].acquisition_value_eur, 40.0));
        assert(UTAX_NEAR(rows[0].sale_value_eur, 60.0));
        assert(UTAX_NEAR(rows[0].costs_eur, 0.0));
    }

    {
        utax_fifo_snapshot_filter f;
        utax_fifo_snapshot_row rows[8];
        size_t out_n = 0, req = 0;
        long long cnt = 0;
        memset(&f, 0, sizeof(f));
        f.has_year = 1;
        f.year = 2025;
        f.year_mode = UTAX_YEAR_EXACT;
        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "IKBR");

        rc = utax_fifo_snapshot_count_filtered(db, &f, &cnt);
        assert(rc == UTAX_OK && cnt == 2);
        rc = utax_fifo_snapshot_get_filtered(db, &f, rows, 8, &out_n, &req);
        assert(rc == UTAX_OK && out_n == 2);

        assert(strcmp(rows[0].acq_datetime, "2025-01-10 10:00") == 0);
        assert(UTAX_NEAR(rows[0].qty_remaining, 15.0));
        assert(UTAX_NEAR(rows[0].cost_per_share_eur, 50.0));
        assert(UTAX_NEAR(rows[0].acq_commission_eur, 2.0));

        assert(strcmp(rows[1].acq_datetime, "2025-02-10 10:00") == 0);
        assert(UTAX_NEAR(rows[1].qty_remaining, 12.0));
        assert(UTAX_NEAR(rows[1].cost_per_share_eur, 55.0));
        assert(UTAX_NEAR(rows[1].acq_commission_eur, 3.0));
    }

    {
        utax_fifo_snapshot_action_applied_filter f;
        long long cnt = 0;
        memset(&f, 0, sizeof(f));
        f.has_lot_id = 1;
        f.lot_id = seeded_lot_id;
        f.has_action_id = 1;
        f.action_id = split_action_id;
        rc = utax_fifo_snapshot_action_applied_count_filtered(db, &f, &cnt);
        assert(rc == UTAX_OK && cnt == 1);
    }

    {
        utax_fifo_realized_row *export_rows = NULL;
        size_t export_n = 0;
        utax_fifo_realized_filter f;
        long long cnt = 0;

        rc = process_year_trades(db, 2025, "IKBR", &export_rows, &export_n);
        assert(rc == UTAX_OK);
        assert(export_n == 3);
        process_year_free_realized_rows(&export_rows, &export_n);

        memset(&f, 0, sizeof(f));
        f.has_year = 1;
        f.year = 2025;
        f.year_mode = UTAX_YEAR_EXACT;
        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "IKBR");
        rc = utax_fifo_realized_count_filtered(db, &f, &cnt);
        assert(rc == UTAX_OK && cnt == 3);

        rc = process_year_trades(db, 2025, NULL, &export_rows, &export_n);
        assert(rc == UTAX_OK);
        assert(export_n == 4);
        process_year_free_realized_rows(&export_rows, &export_n);

        rc = utax_fifo_realized_count_filtered(db, &f, &cnt);
        assert(rc == UTAX_OK && cnt == 3);

        memset(&f, 0, sizeof(f));
        f.has_year = 1;
        f.year = 2025;
        f.year_mode = UTAX_YEAR_EXACT;
        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "REVOLUT");
        rc = utax_fifo_realized_count_filtered(db, &f, &cnt);
        assert(rc == UTAX_OK && cnt == 1);
    }

    rc = utax_db_close(db);
    assert(rc == UTAX_OK);
    remove(db_path);

    printf("All ultimateTax process_year tests passed.\n");
    return 0;
}
