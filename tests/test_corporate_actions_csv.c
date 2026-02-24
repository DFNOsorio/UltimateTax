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
  #define UTAX_FOPEN(out_fp, path, mode) (fopen_s(&(out_fp), (path), (mode)) == 0)
#else
  #define UTAX_STRNCPY(dst, dstsz, src)             \
    do {                                            \
      strncpy((dst), (src), (dstsz) - 1);           \
      (dst)[(dstsz) - 1] = '\0';                    \
    } while (0)
  #define UTAX_FOPEN(out_fp, path, mode) (((out_fp) = fopen((path), (mode))) != NULL)
#endif

#define UTAX_TEST_REQUIRE(cond, msg) \
    do { if (!(cond)) { fprintf(stderr, "TEST FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); abort(); } } while (0)
#define UTAX_TEST_REQUIRE_RC(rc, msg) \
    do { if ((rc) != UTAX_OK) { fprintf(stderr, "TEST FAIL: %s rc=%d (%s:%d)\n", (msg), (int)(rc), __FILE__, __LINE__); abort(); } } while (0)

static void make_temp_path(char *out, size_t out_sz, const char *prefix) {
#if defined(_WIN32)
    char tmpdir[MAX_PATH] = {0};
    DWORD n = GetTempPathA((DWORD)sizeof(tmpdir), tmpdir);
    UTAX_TEST_REQUIRE(n > 0 && n < sizeof(tmpdir), "GetTempPathA failed");

    char tmpfile[MAX_PATH] = {0};
    UINT u = GetTempFileNameA(tmpdir, prefix, 0, tmpfile);
    UTAX_TEST_REQUIRE(u != 0, "GetTempFileNameA failed");

    /* ensure it doesn't exist before we open "wb" */
    DeleteFileA(tmpfile);

    UTAX_STRNCPY(out, out_sz, tmpfile);
#else
    snprintf(out, out_sz, "/tmp/%s_%ld.tmp", prefix, (long)getpid());
#endif
}

static void write_text_file_or_die(const char *path, const char *text) {
    UTAX_TEST_REQUIRE(path && text, "write_text_file invalid args");

    FILE *f = NULL;
    if (!UTAX_FOPEN(f, path, "wb")) {
        fprintf(stderr, "write_text_file: failed to open '%s'\n", path);
        abort();
    }

    size_t n = strlen(text);
    size_t w = fwrite(text, 1, n, f);
    if (w != n) {
        fprintf(stderr, "write_text_file: fwrite failed (%zu/%zu)\n", w, n);
        fclose(f);
        abort();
    }
    fclose(f);
}

int main(int argc, char **argv) {
    UTAX_TEST_REQUIRE(argc >= 2, "schema path missing");
    const char *schema_path = argv[1];

    /* temp DB */
    char db_path[512];
    make_temp_path(db_path, sizeof(db_path), "udb");

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

    /* write CSV */
    char csv_path[512];
    make_temp_path(csv_path, sizeof(csv_path), "csv");
    write_text_file_or_die(csv_path, UTAX_CORP_ACTIONS_CSV_TEXT);

    /* parse -> list */
    utax_corporate_actions_node *head = NULL;
    size_t total = 0;

    rc = utax_corporate_actions_parse_csv_file(csv_path, &head, &total);
    UTAX_TEST_REQUIRE_RC(rc, "parse_csv_file failed");
    UTAX_TEST_REQUIRE(total == 3, "expected 3 parsed rows");
    UTAX_TEST_REQUIRE(head != NULL && head->next != NULL, "expected linked list");

    /* verify SPLIT row has empty to_ticker in node */
    {
        int saw_split = 0;
        for (utax_corporate_actions_node *p = head; p; p = p->next) {
            if (strcmp(p->row.action_type, "SPLIT") == 0) {
                saw_split = 1;
                UTAX_TEST_REQUIRE(p->row.to_ticker[0] == '\0', "split node to_ticker must be empty");
                UTAX_TEST_REQUIRE(p->row.from_qty == 8.0 && p->row.to_qty == 1.0, "split qty mismatch");
            }
        }
        UTAX_TEST_REQUIRE(saw_split, "did not find SPLIT row");
    }

    /* insert list (batch) */
    size_t inserted = 0;
    rc = utax_corporate_actions_insert_many_list(db, head, &inserted);
    if (rc != UTAX_OK) fprintf(stderr, "insert_many_list rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
    UTAX_TEST_REQUIRE_RC(rc, "insert_many_list failed");
    UTAX_TEST_REQUIRE(inserted == 3, "insert_many_list inserted mismatch");

    /* verify ids set */
    for (utax_corporate_actions_node *p = head; p; p = p->next) {
        UTAX_TEST_REQUIRE(p->row.action_id > 0, "node action_id not set");
    }

    /* free list */
    utax_corporate_actions_free_list(&head, &total);
    UTAX_TEST_REQUIRE(head == NULL, "head should be NULL after free");
    UTAX_TEST_REQUIRE(total == 0, "total should be 0 after free");

    /* verify DB count */
    long long ct = -1;
    rc = utax_corporate_actions_count_total(db, &ct);
    UTAX_TEST_REQUIRE_RC(rc, "count_total failed");
    UTAX_TEST_REQUIRE(ct == 3, "expected count_total=3");

    /* insert from file path (parse->insert->free) should add 3 more */
    size_t inserted2 = 0;
    rc = utax_corporate_actions_insert_many_from_csv_file(db, csv_path, &inserted2);
    if (rc != UTAX_OK) fprintf(stderr, "insert_many_from_csv_file rc=%d err=%s\n", (int)rc, utax_db_last_error(db));
    UTAX_TEST_REQUIRE_RC(rc, "insert_many_from_csv_file failed");
    UTAX_TEST_REQUIRE(inserted2 == 3, "insert_many_from_csv_file inserted mismatch");

    rc = utax_corporate_actions_count_total(db, &ct);
    UTAX_TEST_REQUIRE_RC(rc, "count_total failed");
    UTAX_TEST_REQUIRE(ct == 6, "expected count_total=6");

    rc = utax_db_close(db);
    UTAX_TEST_REQUIRE_RC(rc, "db_close failed");

    remove(db_path);
    remove(csv_path);

    printf("All ultimateTax corporate_actions CSV/list tests passed.\n");
    return 0;
}
