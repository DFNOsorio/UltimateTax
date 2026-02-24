#include "utax_corporate_actions.h"
#include "utax_db.h"
#include "utax_fifo_realized.h"
#include "utax_fifo_snapshot.h"
#include "utax_process_year.h"
#include "utax_schema.h"
#include "utax_trades.h"

#include "mock_process_year.h"

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
    snprintf(out, out_sz, "/tmp/utax_process_year_test_%ld.db", (long)getpid());
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

    const size_t trade_n = utax_mock_process_year_trades_count();
    for (size_t i = 0; i < trade_n; ++i) {
        long long id = 0;
        rc = utax_trades_insert(db, &UTAX_MOCK_PROCESS_YEAR_TRADES[i], &id);
        if (rc != UTAX_OK) fprintf(stderr, "trade insert failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);
        assert(id > 0);
    }

    const size_t snap_n = utax_mock_process_year_snapshots_count();
    long long pre_2024_snapshot_lot_id = 0;
    long long stale_2025_snapshot_lot_id = 0;
    for (size_t i = 0; i < snap_n; ++i) {
        utax_fifo_snapshot_row row = UTAX_MOCK_PROCESS_YEAR_SNAPSHOTS[i].row;
        long long acq_id = find_trade_id_by_datetime(db, UTAX_MOCK_PROCESS_YEAR_SNAPSHOTS[i].acq_trade_datetime_key);
        assert(acq_id > 0);
        row.acq_trade_id = acq_id;

        long long lot_id = 0;
        rc = utax_fifo_snapshot_insert(db, &row, &lot_id);
        if (rc != UTAX_OK) fprintf(stderr, "snapshot insert failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);
        assert(lot_id > 0);

        if (row.tax_year == 2024) pre_2024_snapshot_lot_id = lot_id;
        if (row.tax_year == 2025) stale_2025_snapshot_lot_id = lot_id;
    }
    assert(pre_2024_snapshot_lot_id > 0);
    assert(stale_2025_snapshot_lot_id > 0);

    const size_t action_n = utax_mock_process_year_actions_count();
    for (size_t i = 0; i < action_n; ++i) {
        long long action_id = 0;
        rc = utax_corporate_actions_insert(db, &UTAX_MOCK_PROCESS_YEAR_ACTIONS[i], &action_id);
        if (rc != UTAX_OK) fprintf(stderr, "action insert failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);
        assert(action_id > 0);
    }

    /* stale realized row for this broker/year should be cleared by process_year_trades */
    {
        utax_fifo_realized_row stale;
        memset(&stale, 0, sizeof(stale));

        stale.sell_trade_id = find_trade_id_by_datetime(db, "2025-06-01 10:00");
        stale.buy_trade_id = find_trade_id_by_datetime(db, "2025-02-01 10:00");
        assert(stale.sell_trade_id > 0 && stale.buy_trade_id > 0);

        stale.qty_matched = 1.0;
        stale.acquisition_value_eur = 120.0;
        stale.sale_value_eur = 150.0;
        stale.costs_eur = 0.0;
        stale.tax_year = 2025;
        stale.match_seq = 1;

        UTAX_STRNCPY(stale.broker, sizeof(stale.broker), "IKBR");
        UTAX_STRNCPY(stale.ticker, sizeof(stale.ticker), "OLD");
        UTAX_STRNCPY(stale.country, sizeof(stale.country), "US");
        UTAX_STRNCPY(stale.sell_datetime, sizeof(stale.sell_datetime), "2025-06-01 10:00");
        UTAX_STRNCPY(stale.buy_datetime, sizeof(stale.buy_datetime), "2025-02-01 10:00");

        long long rid = 0;
        rc = utax_fifo_realized_insert(db, &stale, &rid);
        if (rc != UTAX_OK) fprintf(stderr, "stale realized insert failed: %s\n", utax_db_last_error(db));
        assert(rc == UTAX_OK);
        assert(rid > 0);
    }

    size_t no_export_total = 1234;
    rc = process_year_trades(db, 2030, NULL, &no_export_total);
    assert(rc == UTAX_OK);
    assert(no_export_total == 1234);

    utax_process_year_realized_node *export_head = NULL;
    size_t export_total = 0;
    rc = process_year_trades(db, 2025, &export_head, &export_total);
    assert(rc == UTAX_OK);
    assert(export_total == 2);

    {
        const utax_process_year_realized_node *n = export_head;
        assert(n != NULL);
        assert(strcmp(n->row.ticker, "ABC") == 0);
        assert(n->row.match_seq == 1);
        n = n->next;
        assert(n != NULL);
        assert(strcmp(n->row.ticker, "ABC") == 0);
        assert(n->row.match_seq == 2);
    }

    {
        utax_fifo_realized_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1;
        f.year = 2025;
        f.year_mode = UTAX_YEAR_EXACT;
        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "IKBR");

        long long cnt = 0;
        rc = utax_fifo_realized_count_filtered(db, &f, &cnt);
        assert(rc == UTAX_OK);
        assert(cnt == 2);

        utax_fifo_realized_row rows[8];
        size_t out_n = 0, req = 0;
        rc = utax_fifo_realized_get_filtered(db, &f, rows, 8, &out_n, &req);
        assert(rc == UTAX_OK);
        assert(out_n == 2);

        assert(strcmp(rows[0].ticker, "ABC") == 0);
        assert(rows[0].match_seq == 1);
        assert(UTAX_NEAR(rows[0].qty_matched, 8.0));
        assert(UTAX_NEAR(rows[0].acquisition_value_eur, 400.0));
        assert(UTAX_NEAR(rows[0].sale_value_eur, 1200.0));
        assert(UTAX_NEAR(rows[0].costs_eur, 10.0));
        assert(strcmp(rows[0].buy_datetime, "2024-01-10 10:00") == 0);

        assert(strcmp(rows[1].ticker, "ABC") == 0);
        assert(rows[1].match_seq == 2);
        assert(UTAX_NEAR(rows[1].qty_matched, 4.0));
        assert(UTAX_NEAR(rows[1].acquisition_value_eur, 240.0));
        assert(UTAX_NEAR(rows[1].sale_value_eur, 600.0));
        assert(UTAX_NEAR(rows[1].costs_eur, 6.0));
        assert(strcmp(rows[1].buy_datetime, "2025-02-01 10:00") == 0);
    }

    {
        utax_fifo_snapshot_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1;
        f.year = 2025;
        f.year_mode = UTAX_YEAR_EXACT;
        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "IKBR");

        long long cnt = 0;
        rc = utax_fifo_snapshot_count_filtered(db, &f, &cnt);
        assert(rc == UTAX_OK);
        assert(cnt == 2);

        utax_fifo_snapshot_row rows[8];
        size_t out_n = 0, req = 0;
        rc = utax_fifo_snapshot_get_filtered(db, &f, rows, 8, &out_n, &req);
        assert(rc == UTAX_OK);
        assert(out_n == 2);

        assert(strcmp(rows[0].ticker, "ABC") == 0);
        assert(strcmp(rows[0].acq_datetime, "2025-02-01 10:00") == 0);
        assert(UTAX_NEAR(rows[0].qty_remaining, 6.0));
        assert(UTAX_NEAR(rows[0].cost_per_share_eur, 60.0));
        assert(UTAX_NEAR(rows[0].acq_commission_eur, 3.0));

        assert(strcmp(rows[1].ticker, "ABC") == 0);
        assert(strcmp(rows[1].acq_datetime, "2025-04-01 10:00") == 0);
        assert(UTAX_NEAR(rows[1].qty_remaining, 2.0));
        assert(UTAX_NEAR(rows[1].cost_per_share_eur, 130.0));
        assert(UTAX_NEAR(rows[1].acq_commission_eur, 0.0));

        for (size_t i = 0; i < out_n; ++i) {
            assert(strcmp(rows[i].ticker, "OLD") != 0);
        }
    }

    rc = utax_fifo_snapshot_delete_by_id(db, stale_2025_snapshot_lot_id);
    assert(rc == UTAX_ERR_NOT_FOUND);

    rc = utax_fifo_snapshot_delete_by_id(db, pre_2024_snapshot_lot_id);
    assert(rc == UTAX_ERR_NOT_FOUND);

    /* run the same processing year again to explicitly validate re-run behavior */
    rc = process_year_trades(db, 2025, NULL, NULL);
    assert(rc == UTAX_OK);

    {
        utax_fifo_realized_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1;
        f.year = 2025;
        f.year_mode = UTAX_YEAR_EXACT;
        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "IKBR");

        long long cnt = 0;
        rc = utax_fifo_realized_count_filtered(db, &f, &cnt);
        assert(rc == UTAX_OK);
        assert(cnt == 2);
    }

    {
        utax_fifo_snapshot_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1;
        f.year = 2025;
        f.year_mode = UTAX_YEAR_EXACT;
        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "IKBR");

        long long cnt = 0;
        rc = utax_fifo_snapshot_count_filtered(db, &f, &cnt);
        assert(rc == UTAX_OK);
        assert(cnt == 0);
    }

    process_year_free_realized_list(&export_head, &export_total);
    assert(export_head == NULL);
    assert(export_total == 0);

    rc = utax_db_close(db);
    assert(rc == UTAX_OK);

    remove(db_path);

    printf("All ultimateTax process_year tests passed.\n");
    return 0;
}
