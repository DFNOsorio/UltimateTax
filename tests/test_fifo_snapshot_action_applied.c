#include "utax_corporate_actions.h"
#include "utax_db.h"
#include "utax_fifo_snapshot.h"
#include "utax_fifo_snapshot_action_applied.h"
#include "utax_schema.h"
#include "utax_trades.h"

#include "mock_fifo_snapshot.h"
#include "mock_fifo_snapshot_action_applied.h"
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
    snprintf(out, out_sz, "/tmp/utax_fifo_snapshot_action_applied_test_%ld.db", (long)getpid());
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

    for (size_t i = 0; i < utax_mock_trades_count(); ++i) {
        long long id = 0;
        rc = utax_trades_insert(db, &UTAX_MOCK_TRADES[i], &id);
        assert(rc == UTAX_OK);
        assert(id > 0);
    }

    long long lot_ids[2] = {0};
    for (size_t i = 0; i < 2; ++i) {
        utax_fifo_snapshot_row row = UTAX_MOCK_FIFO_SNAPSHOTS[i == 0 ? 0 : 2].row;
        long long trade_id = find_trade_id_by_datetime(db, UTAX_MOCK_FIFO_SNAPSHOT_ACTION_APPLIED[i].acq_trade_datetime_key);
        assert(trade_id > 0);
        row.acq_trade_id = trade_id;

        rc = utax_fifo_snapshot_insert(db, &row, &lot_ids[i]);
        assert(rc == UTAX_OK);
        assert(lot_ids[i] > 0);
    }

    const size_t action_n = utax_mock_fifo_snapshot_corporate_actions_count();
    assert(action_n <= 8);
    long long action_ids[8] = {0};
    for (size_t i = 0; i < action_n; ++i) {
        utax_corporate_actions_row a = UTAX_MOCK_FIFO_SNAPSHOT_CORPORATE_ACTIONS[i].row;
        rc = utax_corporate_actions_insert(db, &a, &action_ids[i]);
        assert(rc == UTAX_OK);
        assert(action_ids[i] > 0);
    }

    {
        utax_corporate_actions_filter f;
        memset(&f, 0, sizeof(f));
        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "REVO -> IKBR");

        long long c = -1;
        rc = utax_corporate_actions_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);
        assert(c == 2);
    }

    long long total = -1;
    rc = utax_fifo_snapshot_action_applied_count_total(db, &total);
    assert(rc == UTAX_OK);
    assert(total == 0);

    utax_fifo_snapshot_action_applied_row r0 = { lot_ids[0], action_ids[0] };
    utax_fifo_snapshot_action_applied_row r1 = { lot_ids[1], action_ids[1] };

    rc = utax_fifo_snapshot_action_applied_insert(db, &r0);
    assert(rc == UTAX_OK);

    rc = utax_fifo_snapshot_action_applied_insert(db, &r0);
    assert(rc != UTAX_OK);

    rc = utax_fifo_snapshot_action_applied_count_total(db, &total);
    assert(rc == UTAX_OK);
    assert(total == 1);

    {
        utax_fifo_snapshot_action_applied_row bad_batch[2];
        bad_batch[0] = r1;
        bad_batch[1] = r0;

        size_t done = 0;
        rc = utax_fifo_snapshot_action_applied_insert_many(db, bad_batch, 2, &done);
        assert(rc != UTAX_OK);
        assert(done <= 1);

        rc = utax_fifo_snapshot_action_applied_count_total(db, &total);
        assert(rc == UTAX_OK);
        assert(total == 1);
    }

    {
        utax_fifo_snapshot_action_applied_row good_batch[1];
        good_batch[0] = r1;
        size_t done = 0;
        rc = utax_fifo_snapshot_action_applied_insert_many(db, good_batch, 1, &done);
        assert(rc == UTAX_OK);
        assert(done == 1);

        rc = utax_fifo_snapshot_action_applied_count_total(db, &total);
        assert(rc == UTAX_OK);
        assert(total == 2);
    }

    {
        utax_fifo_snapshot_action_applied_row upd = { lot_ids[1], action_ids[2] };
        rc = utax_fifo_snapshot_action_applied_update_by_keys(db, lot_ids[1], action_ids[1], &upd);
        assert(rc == UTAX_OK);

        utax_fifo_snapshot_action_applied_filter f;
        memset(&f, 0, sizeof(f));
        f.has_lot_id = 1;
        f.lot_id = lot_ids[1];
        f.has_action_id = 1;
        f.action_id = action_ids[1];

        long long c = -1;
        rc = utax_fifo_snapshot_action_applied_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);
        assert(c == 0);

        f.action_id = action_ids[2];
        rc = utax_fifo_snapshot_action_applied_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);
        assert(c == 1);
    }

    rc = utax_fifo_snapshot_action_applied_delete_by_keys(db, lot_ids[0], action_ids[0]);
    assert(rc == UTAX_OK);

    rc = utax_fifo_snapshot_action_applied_count_total(db, &total);
    assert(rc == UTAX_OK);
    assert(total == 1);

    rc = utax_fifo_snapshot_delete_by_id(db, lot_ids[1]);
    assert(rc == UTAX_OK);

    {
        utax_fifo_snapshot_action_applied_filter f;
        memset(&f, 0, sizeof(f));
        f.has_lot_id = 1;
        f.lot_id = lot_ids[1];

        long long c = -1;
        rc = utax_fifo_snapshot_action_applied_count_filtered(db, &f, &c);
        assert(rc == UTAX_OK);
        assert(c == 0);
    }

    rc = utax_fifo_snapshot_action_applied_count_total(db, &total);
    assert(rc == UTAX_OK);
    assert(total == 0);

    rc = utax_db_close(db);
    assert(rc == UTAX_OK);

    remove(db_path);

    printf("All ultimateTax fifo_snapshot_action_applied tests passed.\n");
    return 0;
}
