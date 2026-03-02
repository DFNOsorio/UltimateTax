#include "utax_db.h"
#include "utax_schema.h"
#include "utax_corporate_actions.h"
#include "mock_corporate_actions.h"

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

#define UTAX_TEST_REQUIRE(cond, msg) \
    do { if (!(cond)) { fprintf(stderr, "TEST FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); abort(); } } while (0)

#define UTAX_TEST_REQUIRE_RC(rc, msg) \
    do { if ((rc) != UTAX_OK) { fprintf(stderr, "TEST FAIL: %s rc=%d (%s:%d)\n", (msg), (int)(rc), __FILE__, __LINE__); abort(); } } while (0)

static void make_temp_db_path(char *out, size_t out_sz) {
#if defined(_WIN32)
    char tmpdir[MAX_PATH] = {0};
    DWORD n = GetTempPathA((DWORD)sizeof(tmpdir), tmpdir);
    UTAX_TEST_REQUIRE(n > 0 && n < sizeof(tmpdir), "GetTempPathA failed");

    char tmpfile[MAX_PATH] = {0};
    UINT u = GetTempFileNameA(tmpdir, "utx", 0, tmpfile);
    UTAX_TEST_REQUIRE(u != 0, "GetTempFileNameA failed");

    /* avoid any weirdness with existing file */
    DeleteFileA(tmpfile);

    UTAX_STRNCPY(out, out_sz, tmpfile);
#else
    snprintf(out, out_sz, "/tmp/utax_ca_test_%ld.db", (long)getpid());
#endif
}

static int matches_filter_row(const utax_corporate_actions_row *r, const utax_corporate_actions_filter *f) {
    if (!f) return 1;

    /* derive year from action_date */
    int y = 0;
    if (strlen(r->action_date) >= 4) {
        char yy[5] = { r->action_date[0], r->action_date[1], r->action_date[2], r->action_date[3], 0 };
        y = atoi(yy);
    }

    if (f->has_year) {
        if (f->year_mode == UTAX_YEAR_UP_TO) { if (!(y <= f->year)) return 0; }
        else { if (!(y == f->year)) return 0; }
    }
    if (f->has_broker && strcmp(r->broker, f->broker) != 0) return 0;
    if (f->has_from_ticker && strcmp(r->from_ticker, f->from_ticker) != 0) return 0;

    return 1;
}

static size_t expected_count(const utax_corporate_actions_row *arr, size_t n, const utax_corporate_actions_filter *f) {
    size_t c = 0;
    for (size_t i = 0; i < n; ++i) if (matches_filter_row(&arr[i], f)) c++;
    return c;
}

static size_t expected_page(size_t total, const utax_corporate_actions_filter *f) {
    if (!f) return total;
    int offset = f->has_offset ? f->offset : 0;
    int limit  = f->has_limit  ? f->limit  : -1;

    if (offset < 0) offset = 0;
    if ((size_t)offset >= total) return 0;

    size_t rem = total - (size_t)offset;
    if (limit < 0) return rem;
    return rem < (size_t)limit ? rem : (size_t)limit;
}

int main(int argc, char **argv) {
    UTAX_TEST_REQUIRE(argc >= 2, "schema path missing");
    const char *schema_path = argv[1];

    char db_path[512];
    make_temp_db_path(db_path, sizeof(db_path));

    utax_db_open_opts opts = utax_db_open_opts_default();
    opts.create_if_missing = 1;
    opts.read_only = 0;
    opts.busy_timeout_ms = 50;

    utax_db_t *db = NULL;
    utax_rc rc = utax_db_open(db_path, &opts, &db);
    UTAX_TEST_REQUIRE_RC(rc, "db_open failed");

    rc = utax_schema_apply_from_file(db, schema_path);
    if (rc != UTAX_OK) fprintf(stderr, "schema_apply rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
    UTAX_TEST_REQUIRE_RC(rc, "schema_apply failed");

    /* count total initially */
    long long ct = -1;
    rc = utax_corporate_actions_count_total(db, &ct);
    UTAX_TEST_REQUIRE_RC(rc, "count_total failed");
    UTAX_TEST_REQUIRE(ct == 0, "expected empty table");

    /* batch insert from array */
    const size_t N = utax_mock_corp_actions_count();
    UTAX_TEST_REQUIRE(N <= 32, "too many mocks");

    utax_corporate_actions_row batch[32];
    memset(batch, 0, sizeof(batch));
    for (size_t i = 0; i < N; ++i) batch[i] = UTAX_MOCK_CORP_ACTIONS[i];

    size_t inserted = 0;
    rc = utax_corporate_actions_insert_many(db, batch, N, &inserted);
    if (rc != UTAX_OK) fprintf(stderr, "insert_many rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
    UTAX_TEST_REQUIRE_RC(rc, "insert_many failed");
    UTAX_TEST_REQUIRE(inserted == N, "inserted count mismatch");
    for (size_t i = 0; i < N; ++i) UTAX_TEST_REQUIRE(batch[i].action_id > 0, "action_id not set");

    rc = utax_corporate_actions_count_total(db, &ct);
    UTAX_TEST_REQUIRE_RC(rc, "count_total failed");
    UTAX_TEST_REQUIRE((size_t)ct == N, "count_total mismatch");

    /* count_filtered */
    {
        utax_corporate_actions_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1; f.year = 2020; f.year_mode = UTAX_YEAR_EXACT;

        long long c = -1;
        rc = utax_corporate_actions_count_filtered(db, &f, &c);
        UTAX_TEST_REQUIRE_RC(rc, "count_filtered failed");

        size_t exp = expected_count(UTAX_MOCK_CORP_ACTIONS, N, &f);
        UTAX_TEST_REQUIRE((size_t)c == exp, "count_filtered(2020) mismatch");
    }
    {
        utax_corporate_actions_filter f;
        memset(&f, 0, sizeof(f));
        f.has_broker = 1;
        UTAX_STRNCPY(f.broker, sizeof(f.broker), "REVO");

        long long c = -1;
        rc = utax_corporate_actions_count_filtered(db, &f, &c);
        UTAX_TEST_REQUIRE_RC(rc, "count_filtered failed");

        size_t exp = expected_count(UTAX_MOCK_CORP_ACTIONS, N, &f);
        UTAX_TEST_REQUIRE((size_t)c == exp, "count_filtered(broker) mismatch");
    }
    {
        utax_corporate_actions_filter f;
        memset(&f, 0, sizeof(f));
        f.has_from_ticker = 1;
        UTAX_STRNCPY(f.from_ticker, sizeof(f.from_ticker), "GE");

        long long c = -1;
        rc = utax_corporate_actions_count_filtered(db, &f, &c);
        UTAX_TEST_REQUIRE_RC(rc, "count_filtered failed");

        size_t exp = expected_count(UTAX_MOCK_CORP_ACTIONS, N, &f);
        UTAX_TEST_REQUIRE((size_t)c == exp, "count_filtered(from_ticker) mismatch");
    }

    /* get_filtered all */
    utax_corporate_actions_row out[32];
    size_t out_n = 0, req = 0;
    {
        utax_corporate_actions_filter f;
        memset(&f, 0, sizeof(f));

        rc = utax_corporate_actions_get_filtered(db, &f, out, 32, &out_n, &req);
        if (rc != UTAX_OK) fprintf(stderr, "get_filtered rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
        UTAX_TEST_REQUIRE_RC(rc, "get_filtered(all) failed");
        UTAX_TEST_REQUIRE(out_n == N && req == N, "get_filtered(all) size mismatch");

        /* ordering by date ascending */
        for (size_t i = 1; i < out_n; ++i) {
            UTAX_TEST_REQUIRE(strcmp(out[i-1].action_date, out[i].action_date) <= 0, "ordering mismatch");
        }
        /* SPLIT/CASH rows should have to_ticker empty string when read back */
        for (size_t i = 0; i < out_n; ++i) {
            if (strcmp(out[i].action_type, "SPLIT") == 0 || strcmp(out[i].action_type, "CASH") == 0) {
                UTAX_TEST_REQUIRE(out[i].to_ticker[0] == '\0', "SPLIT/CASH must have empty to_ticker");
            } else {
                UTAX_TEST_REQUIRE(out[i].to_ticker[0] != '\0', "non-SPLIT/CASH must have to_ticker");
            }
            UTAX_TEST_REQUIRE(out[i].ratio > 0.0, "ratio should be computed in DB");
        }
    }

    /* capacity too small (uses count_page) */
    {
        utax_corporate_actions_filter f;
        memset(&f, 0, sizeof(f));
        f.has_year = 1; f.year = 2021; f.year_mode = UTAX_YEAR_UP_TO;

        utax_corporate_actions_row tiny[1];
        size_t oc = 0, rq = 0;

        rc = utax_corporate_actions_get_filtered(db, &f, tiny, 1, &oc, &rq);
        UTAX_TEST_REQUIRE(rc == UTAX_ERR_NO_SPACE, "expected UTAX_ERR_NO_SPACE");
        UTAX_TEST_REQUIRE(oc == 0, "expected 0 out_count on no-space");
        UTAX_TEST_REQUIRE(rq > 1, "expected required > 1");
    }

    /* pagination */
    {
        utax_corporate_actions_filter f;
        memset(&f, 0, sizeof(f));
        f.has_limit = 1; f.limit = 2;
        f.has_offset = 1; f.offset = 1;

        utax_corporate_actions_row page[2];
        size_t oc = 0, rq = 0;

        rc = utax_corporate_actions_get_filtered(db, &f, page, 2, &oc, &rq);
        if (rc != UTAX_OK) fprintf(stderr, "get_filtered(page) rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
        UTAX_TEST_REQUIRE_RC(rc, "get_filtered(page) failed");

        size_t total = expected_count(UTAX_MOCK_CORP_ACTIONS, N, NULL);
        size_t exp_page = expected_page(total, &f);
        UTAX_TEST_REQUIRE(oc == exp_page && rq == exp_page, "pagination size mismatch");
    }

    /* update_by_id on the first inserted row */
    {
        utax_corporate_actions_row upd = batch[0];
        UTAX_STRNCPY(upd.broker, sizeof(upd.broker), "IKBR");
        upd.from_qty = 10.0;
        upd.to_qty = 20.0;

        rc = utax_corporate_actions_update_by_id(db, batch[0].action_id, &upd);
        if (rc != UTAX_OK) fprintf(stderr, "update rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
        UTAX_TEST_REQUIRE_RC(rc, "update_by_id failed");

        utax_corporate_actions_filter all;
        memset(&all, 0, sizeof(all));

        size_t fn = 0, fr = 0;
        rc = utax_corporate_actions_get_filtered(db, &all, out, 32, &fn, &fr);
        UTAX_TEST_REQUIRE_RC(rc, "get_filtered after update failed");

        int found = 0;
        for (size_t i = 0; i < fn; ++i) {
            if (out[i].action_id == batch[0].action_id) {
                found = 1;
                UTAX_TEST_REQUIRE(strcmp(out[i].broker, "IKBR") == 0, "update broker mismatch");
                UTAX_TEST_REQUIRE(out[i].from_qty == 10.0 && out[i].to_qty == 20.0, "update qty mismatch");
                UTAX_TEST_REQUIRE(out[i].ratio == 2.0, "update ratio mismatch");
            }
        }
        UTAX_TEST_REQUIRE(found, "updated row not found");
    }

    /* delete_by_id */
    {
        long long before = 0, after = 0;
        rc = utax_corporate_actions_count_total(db, &before);
        UTAX_TEST_REQUIRE_RC(rc, "count_total before delete failed");

        rc = utax_corporate_actions_delete_by_id(db, batch[0].action_id);
        if (rc != UTAX_OK) fprintf(stderr, "delete rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
        UTAX_TEST_REQUIRE_RC(rc, "delete_by_id failed");

        rc = utax_corporate_actions_count_total(db, &after);
        UTAX_TEST_REQUIRE_RC(rc, "count_total after delete failed");
        UTAX_TEST_REQUIRE(after == before - 1, "delete did not decrement count");

        /* deleting again should return NOT_FOUND */
        rc = utax_corporate_actions_delete_by_id(db, batch[0].action_id);
        UTAX_TEST_REQUIRE(rc == UTAX_ERR_NOT_FOUND, "expected NOT_FOUND on second delete");
    }

    rc = utax_db_close(db);
    UTAX_TEST_REQUIRE_RC(rc, "db_close failed");

    remove(db_path);

    printf("All ultimateTax corporate_actions tests passed.\n");
    return 0;
}
