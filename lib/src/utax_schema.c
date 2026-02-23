#include "utax_schema.h"
#include "utax_db.h"
#include "utax_db_priv.h"

#include <sqlite3.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static utax_rc utax__read_file_all(const char *path, char **out_buf){
    if(!path || !out_buf) return UTAX_ERR_INVALID_ARG;

    *out_buf = NULL;
    FILE *f = NULL;

    if (!UTAX_FOPEN(f, path, "rb")) return UTAX_ERR_SQLITE;

    if(fseek(f, 0, SEEK_END) != 0) { fclose(f); return UTAX_ERR_SQLITE;}
    long sz = ftell(f);
    if(sz < 0) {fclose(f); return UTAX_ERR_SQLITE;}
    rewind(f);

    char *buf = (char *) malloc((size_t) sz+1);
    if (!buf) {fclose(f); return UTAX_ERR_SQLITE;}

    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if(n != (size_t)sz) {free(buf); return UTAX_ERR_SQLITE;}
    buf[sz] = '\0';
    *out_buf = buf;
    return UTAX_OK;
}

static utax_rc utax__exec(struct utax_db *h, const char *sql){
    char *errmsg = NULL;
    int rc =  sqlite3_exec(h->db, sql, NULL, NULL, &errmsg);
    if(rc != SQLITE_OK) {
        if(errmsg) {
            UTAX_STRNCPY(h->last_errmsg, sizeof(h->last_errmsg), errmsg);
            sqlite3_free(errmsg);
        } else {
            utax__set_err_sqlite(h, rc);
        }

        return UTAX_ERR_SQLITE;
    }

    return UTAX_OK;
}

utax_rc utax_schema_apply_from_file(utax_db_t *db, const char *schema_sql_path){
    if(!db || !schema_sql_path) return  UTAX_ERR_INVALID_ARG;

    struct utax_db *h = (struct utax_db *) db;

    char *sql = NULL;
    utax_rc rc = utax__read_file_all(schema_sql_path, &sql);
    if (rc != UTAX_OK) {
        UTAX_STRNCPY(h->last_errmsg, sizeof(h->last_errmsg), "failed to read schema file");
        return rc;
    }

    rc = utax__exec(h, "BEGIN;");
    if(rc != UTAX_OK) {free(sql); return rc;}

    rc = utax__exec(h, sql);
    free(sql);

    if(rc != UTAX_OK) {
        (void) utax__exec(h, "ROLLBACK;");
        return rc;
    }

    rc = utax__exec(h, "COMMIT;");
    if(rc != UTAX_OK) {
        (void) utax__exec(h, "ROLLBACK;");
        return rc;
    }

    return UTAX_OK;
}

static utax_rc utax__drop_objects_of_type(struct utax_db *h, const char *type) {
    const char *q =
        "SELECT name FROM sqlite_master "
        "WHERE type = ?1 AND name NOT LIKE 'sqlite_%' "
        "ORDER BY name;";

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(h->db, q, -1, &st, NULL);
    if (rc != SQLITE_OK) return utax__set_err_sqlite(h, rc);

    rc = sqlite3_bind_text(st, 1, type, -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(st);
        return utax__set_err_sqlite(h, rc);
    }

    /* Phase 1: collect names */
    char **names = NULL;
    size_t count = 0;
    size_t cap = 0;

    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const unsigned char *name_u = sqlite3_column_text(st, 0);
        if (!name_u) continue;

        const char *name = (const char *)name_u;

        if (count == cap) {
            size_t new_cap = (cap == 0) ? 16 : cap * 2;
            char **new_names = (char **)realloc(names, new_cap * sizeof(char *));
            if (!new_names) {
                sqlite3_finalize(st);
                for (size_t i = 0; i < count; ++i) free(names[i]);
                free(names);
                return UTAX_ERR_NOMEM;
            }
            names = new_names;
            cap = new_cap;
        }

#if defined(_MSC_VER)
        names[count] = _strdup(name);
#else
        names[count] = strdup(name);
#endif
        if (!names[count]) {
            sqlite3_finalize(st);
            for (size_t i = 0; i < count; ++i) free(names[i]);
            free(names);
            return UTAX_ERR_NOMEM;
        }
        count++;
    }

    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) {
        for (size_t i = 0; i < count; ++i) free(names[i]);
        free(names);
        return utax__set_err_sqlite(h, rc);
    }

    /* Phase 2: drop after statement is finalized (prevents schema lock issues) */
    for (size_t i = 0; i < count; ++i) {
        char *drop_sql = sqlite3_mprintf("DROP %s \"%w\";", type, names[i]);
        if (!drop_sql) {
            for (size_t j = 0; j < count; ++j) free(names[j]);
            free(names);
            return UTAX_ERR_NOMEM;
        }

        utax_rc drc = utax__exec(h, drop_sql);
        sqlite3_free(drop_sql);

        if (drc != UTAX_OK) {
            for (size_t j = 0; j < count; ++j) free(names[j]);
            free(names);
            return drc;
        }
    }

    for (size_t i = 0; i < count; ++i) free(names[i]);
    free(names);
    return UTAX_OK;
}

utax_rc utax_schema_drop_all(utax_db_t *db) {
    if (!db) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    utax_rc rc = utax__exec(h, "PRAGMA foreign_keys=OFF;");
    if (rc != UTAX_OK) return rc;

    rc = utax__exec(h, "BEGIN;");
    if (rc != UTAX_OK) return rc;

    /* Order matters */
    rc = utax__drop_objects_of_type(h, "trigger");
    if (rc == UTAX_OK) rc = utax__drop_objects_of_type(h, "view");
    if (rc == UTAX_OK) rc = utax__drop_objects_of_type(h, "table");

    if (rc != UTAX_OK) {
        (void)utax__exec(h, "ROLLBACK;");
        (void)utax__exec(h, "PRAGMA foreign_keys=ON;");
        return rc;
    }

    /* Reset AUTOINCREMENT counters if present */
    (void)utax__exec(h, "DELETE FROM sqlite_sequence;");

    rc = utax__exec(h, "COMMIT;");
    if (rc != UTAX_OK) {
        (void)utax__exec(h, "ROLLBACK;");
        (void)utax__exec(h, "PRAGMA foreign_keys=ON;");
        return rc;
    }

    (void)utax__exec(h, "PRAGMA foreign_keys=ON;");
    return UTAX_OK;
}

utax_rc utax_schema_recreate_from_file(utax_db_t *db, const char *schema_sql_path) {
    if (!db || !schema_sql_path) return UTAX_ERR_INVALID_ARG;

    utax_rc rc = utax_schema_drop_all(db);
    if (rc != UTAX_OK) return rc;

    return utax_schema_apply_from_file(db, schema_sql_path);
}

utax_rc utax_schema_get_user_version(utax_db_t *db, int *out_version) {
    if (!db || !out_version) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(h->db, "PRAGMA user_version;", -1, &st, NULL);
    if (rc != SQLITE_OK) return utax__set_err_sqlite(h, rc);

    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        *out_version = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
        return UTAX_OK;
    }

    sqlite3_finalize(st);
    return utax__set_err_sqlite(h, rc);
}

utax_rc utax_schema_set_user_version(utax_db_t *db, int version) {
    if (!db || version < 0) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[64];
    snprintf(sql, sizeof(sql), "PRAGMA user_version=%d;", version);
    return utax__exec(h, sql);
}
