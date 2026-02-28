#include "utax_fifo_snapshot.h"
#include "utax_db_priv.h"
#include "utax_market_data.h"

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

static utax_rc utax__build_where_plain(char *sql, size_t sql_sz, const utax_fifo_snapshot_filter *f) {
    if (!f) return UTAX_OK;

    if (f->has_year) {
        if (f->year_mode == UTAX_YEAR_UP_TO) {
            if (!UTAX_STRCAT(sql, sql_sz, " AND tax_year <= ?")) return UTAX_ERR_INVALID_ARG;
        } else {
            if (!UTAX_STRCAT(sql, sql_sz, " AND tax_year = ?")) return UTAX_ERR_INVALID_ARG;
        }
    }
    if (f->has_broker)  { if (!UTAX_STRCAT(sql, sql_sz, " AND broker = ?"))  return UTAX_ERR_INVALID_ARG; }
    if (f->has_ticker)  { if (!UTAX_STRCAT(sql, sql_sz, " AND ticker = ?"))  return UTAX_ERR_INVALID_ARG; }
    if (f->has_country) { if (!UTAX_STRCAT(sql, sql_sz, " AND country = ?")) return UTAX_ERR_INVALID_ARG; }

    return UTAX_OK;
}

static utax_rc utax__bind_filters(struct utax_db *h, sqlite3_stmt *st, const utax_fifo_snapshot_filter *f, int *io_idx) {
    int idx = *io_idx;
    if (!f) return UTAX_OK;

    if (f->has_year) {
        if (sqlite3_bind_int(st, idx++, f->year) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f->has_broker) {
        if (sqlite3_bind_text(st, idx++, f->broker, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f->has_ticker) {
        if (sqlite3_bind_text(st, idx++, f->ticker, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    if (f->has_country) {
        if (sqlite3_bind_text(st, idx++, f->country, -1, SQLITE_TRANSIENT) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }

    *io_idx = idx;
    return UTAX_OK;
}

static utax_rc utax__bind_pagination(struct utax_db *h, sqlite3_stmt *st, const utax_fifo_snapshot_filter *f, int *io_idx) {
    if (!f) return UTAX_OK;

    if (f->has_limit || f->has_offset) {
        int limit  = f->has_limit ? f->limit : -1;
        int offset = f->has_offset ? f->offset : 0;

        if (limit < -1 || offset < 0) {
            utax__set_err_msg(h, "invalid pagination");
            return UTAX_ERR_INVALID_ARG;
        }

        if (sqlite3_bind_int(st, (*io_idx)++, limit) != SQLITE_OK)  return utax__set_err_sqlite(h, SQLITE_ERROR);
        if (sqlite3_bind_int(st, (*io_idx)++, offset) != SQLITE_OK) return utax__set_err_sqlite(h, SQLITE_ERROR);
    }
    return UTAX_OK;
}

static void utax__col_text(sqlite3_stmt *st, int col, char *dst, size_t dst_sz) {
    const unsigned char *t = sqlite3_column_text(st, col);
    if (!t) { if (dst_sz) dst[0] = '\0'; return; }
    UTAX_STRNCPY(dst, dst_sz, (const char *)t);
}

static utax_rc utax__update_price_by_lot_stmt(struct utax_db *h,
                                               sqlite3_stmt *st,
                                               long long lot_id,
                                               const char *date_yyyy_mm_dd,
                                               double price)
{
    sqlite3_clear_bindings(st);
    sqlite3_reset(st);

    (void)utax__bind_text(st, 1, date_yyyy_mm_dd);
    sqlite3_bind_double(st, 2, price);
    sqlite3_bind_int64(st, 3, (sqlite3_int64)lot_id);

    {
        int s = sqlite3_step(st);
        if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);
    }

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "fifo_snapshot lot_id not found");
        return UTAX_ERR_NOT_FOUND;
    }

    return UTAX_OK;
}

static int utax__market_lookup_unavailable(utax_rc rc) {
    return rc != UTAX_OK;
}

/* ───────────────────────────── CRUD ───────────────────────────── */

utax_rc utax_fifo_snapshot_insert(utax_db_t *db, const utax_fifo_snapshot_row *row, long long *out_lot_id) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "INSERT INTO fifo_snapshot ("
        " broker, tax_year, ticker, acq_trade_id, acq_datetime, "
        " qty_remaining, cost_per_share_eur, acq_commission_eur, "
        " last_price_update_date, last_updated_stock_price, country"
        ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11);";

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    (void)utax__bind_text(st, 1, row->broker);
    sqlite3_bind_int(st, 2, row->tax_year);
    (void)utax__bind_text(st, 3, row->ticker);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)row->acq_trade_id);
    (void)utax__bind_text(st, 5, row->acq_datetime);

    sqlite3_bind_double(st, 6, row->qty_remaining);
    sqlite3_bind_double(st, 7, row->cost_per_share_eur);
    sqlite3_bind_double(st, 8, row->acq_commission_eur);
    (void)utax__bind_text(st, 9, row->last_price_update_date[0] ? row->last_price_update_date : "");
    sqlite3_bind_double(st, 10, row->last_updated_stock_price);

    (void)utax__bind_text(st, 11, row->country);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (out_lot_id) *out_lot_id = (long long)sqlite3_last_insert_rowid(h->db);
    return UTAX_OK;
}

utax_rc utax_fifo_snapshot_insert_many(utax_db_t *db,
                                       utax_fifo_snapshot_row *rows,
                                       size_t n,
                                       size_t *out_inserted)
{
    if (!db || (!rows && n != 0)) return UTAX_ERR_INVALID_ARG;
    if (out_inserted) *out_inserted = 0;

    if (n == 0) return UTAX_OK;

    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "INSERT INTO fifo_snapshot ("
        " broker, tax_year, ticker, acq_trade_id, acq_datetime, "
        " qty_remaining, cost_per_share_eur, acq_commission_eur, "
        " last_price_update_date, last_updated_stock_price, country"
        ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11);";

    /* Begin transaction (DEFERRED) */
    {
        int rc0 = sqlite3_exec(h->db, "BEGIN;", NULL, NULL, NULL);
        if (rc0 != SQLITE_OK) return utax__set_err_sqlite(h, rc0);
    }

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) {
        (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
        return rc;
    }

    size_t i = 0;
    for (; i < n; ++i) {
        utax_fifo_snapshot_row *r = &rows[i];

        sqlite3_clear_bindings(st);
        sqlite3_reset(st);

        (void)utax__bind_text(st, 1, r->broker);
        sqlite3_bind_int(st, 2, r->tax_year);
        (void)utax__bind_text(st, 3, r->ticker);
        sqlite3_bind_int64(st, 4, (sqlite3_int64)r->acq_trade_id);
        (void)utax__bind_text(st, 5, r->acq_datetime);

        sqlite3_bind_double(st, 6, r->qty_remaining);
        sqlite3_bind_double(st, 7, r->cost_per_share_eur);
        sqlite3_bind_double(st, 8, r->acq_commission_eur);
        (void)utax__bind_text(st, 9, r->last_price_update_date[0] ? r->last_price_update_date : "");
        sqlite3_bind_double(st, 10, r->last_updated_stock_price);

        (void)utax__bind_text(st, 11, r->country);

        int s = sqlite3_step(st);
        if (s != SQLITE_DONE) {
            sqlite3_finalize(st);
            (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
            if (out_inserted) *out_inserted = i;
            return utax__set_err_sqlite(h, s);
        }

        r->lot_id = (long long)sqlite3_last_insert_rowid(h->db);
    }

    sqlite3_finalize(st);

    {
        int rc1 = sqlite3_exec(h->db, "COMMIT;", NULL, NULL, NULL);
        if (rc1 != SQLITE_OK) {
            (void)sqlite3_exec(h->db, "ROLLBACK;", NULL, NULL, NULL);
            if (out_inserted) *out_inserted = i;
            return utax__set_err_sqlite(h, rc1);
        }
    }

    if (out_inserted) *out_inserted = n;
    return UTAX_OK;
}

utax_rc utax_fifo_snapshot_update_by_id(utax_db_t *db, long long lot_id, const utax_fifo_snapshot_row *row) {
    if (!db || !row) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    const char *sql =
        "UPDATE fifo_snapshot SET "
        " broker=?1, tax_year=?2, ticker=?3, acq_trade_id=?4, acq_datetime=?5, "
        " qty_remaining=?6, cost_per_share_eur=?7, acq_commission_eur=?8, "
        " last_price_update_date=?9, last_updated_stock_price=?10, country=?11 "
        "WHERE lot_id=?12;";

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    (void)utax__bind_text(st, 1, row->broker);
    sqlite3_bind_int(st, 2, row->tax_year);
    (void)utax__bind_text(st, 3, row->ticker);
    sqlite3_bind_int64(st, 4, (sqlite3_int64)row->acq_trade_id);
    (void)utax__bind_text(st, 5, row->acq_datetime);

    sqlite3_bind_double(st, 6, row->qty_remaining);
    sqlite3_bind_double(st, 7, row->cost_per_share_eur);
    sqlite3_bind_double(st, 8, row->acq_commission_eur);
    (void)utax__bind_text(st, 9, row->last_price_update_date[0] ? row->last_price_update_date : "");
    sqlite3_bind_double(st, 10, row->last_updated_stock_price);

    (void)utax__bind_text(st, 11, row->country);

    sqlite3_bind_int64(st, 12, (sqlite3_int64)lot_id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "fifo_snapshot lot_id not found");
        return UTAX_ERR_NOT_FOUND;
    }

    return UTAX_OK;
}

utax_rc utax_fifo_snapshot_delete_by_id(utax_db_t *db, long long lot_id) {
    if (!db) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, "DELETE FROM fifo_snapshot WHERE lot_id=?1;");
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_int64(st, 1, (sqlite3_int64)lot_id);

    int s = sqlite3_step(st);
    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    if (sqlite3_changes(h->db) == 0) {
        utax__set_err_msg(h, "fifo_snapshot lot_id not found");
        return UTAX_ERR_NOT_FOUND;
    }

    return UTAX_OK;
}

/* ───────────────────────────── Counts ───────────────────────────── */

utax_rc utax_fifo_snapshot_count_total(utax_db_t *db, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    sqlite3_stmt *st = NULL;
    utax_rc rc = utax__prep(h, &st, "SELECT COUNT(*) FROM fifo_snapshot;");
    if (rc != UTAX_OK) return rc;

    int s = sqlite3_step(st);
    if (s == SQLITE_ROW) {
        *out_count = (long long)sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
        return UTAX_OK;
    }

    sqlite3_finalize(st);
    return utax__set_err_sqlite(h, s);
}

utax_rc utax_fifo_snapshot_count_filtered(utax_db_t *db, const utax_fifo_snapshot_filter *f, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[768];
    UTAX_STRNCPY(sql, sizeof(sql), "SELECT COUNT(*) FROM fifo_snapshot WHERE 1=1");

    utax_rc rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    sqlite3_stmt *st = NULL;
    rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    int idx = 1;
    rc = utax__bind_filters(h, st, f, &idx);
    if (rc != UTAX_OK) { sqlite3_finalize(st); return rc; }

    int s = sqlite3_step(st);
    if (s == SQLITE_ROW) {
        *out_count = (long long)sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
        return UTAX_OK;
    }

    sqlite3_finalize(st);
    return utax__set_err_sqlite(h, s);
}

utax_rc utax_fifo_snapshot_count_page(utax_db_t *db, const utax_fifo_snapshot_filter *f, long long *out_count) {
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    struct utax_db *h = (struct utax_db *)db;

    char sql[1024];
    UTAX_STRNCPY(sql, sizeof(sql),
        "SELECT COUNT(*) FROM ("
        " SELECT lot_id FROM fifo_snapshot WHERE 1=1"
    );

    utax_rc rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY acq_datetime ASC, lot_id ASC")) return UTAX_ERR_INVALID_ARG;

    int want_pagination = (f && (f->has_limit || f->has_offset)) ? 1 : 0;
    if (want_pagination) {
        if (!UTAX_STRCAT(sql, sizeof(sql), " LIMIT ? OFFSET ?")) return UTAX_ERR_INVALID_ARG;
    }

    if (!UTAX_STRCAT(sql, sizeof(sql), " );")) return UTAX_ERR_INVALID_ARG;

    sqlite3_stmt *st = NULL;
    rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    int idx = 1;
    rc = utax__bind_filters(h, st, f, &idx);
    if (rc != UTAX_OK) { sqlite3_finalize(st); return rc; }

    rc = utax__bind_pagination(h, st, f, &idx);
    if (rc != UTAX_OK) { sqlite3_finalize(st); return rc; }

    int s = sqlite3_step(st);
    if (s == SQLITE_ROW) {
        *out_count = (long long)sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
        return UTAX_OK;
    }

    sqlite3_finalize(st);
    return utax__set_err_sqlite(h, s);
}

/* ───────────────────────────── Query rows ───────────────────────────── */

utax_rc utax_fifo_snapshot_get_filtered(utax_db_t *db,
                                        const utax_fifo_snapshot_filter *f,
                                        utax_fifo_snapshot_row *out_rows,
                                        size_t out_cap,
                                        size_t *out_count,
                                        size_t *out_required)
{
    if (!db || !out_count) return UTAX_ERR_INVALID_ARG;
    if (!out_rows && out_cap != 0) return UTAX_ERR_INVALID_ARG;

    *out_count = 0;
    if (out_required) *out_required = 0;

    struct utax_db *h = (struct utax_db *)db;

    long long needed_ll = 0;
    utax_rc rc = utax_fifo_snapshot_count_page(db, f, &needed_ll);
    if (rc != UTAX_OK) return rc;

    size_t needed = (needed_ll <= 0) ? 0 : (size_t)needed_ll;
    if (out_required) *out_required = needed;

    if (needed > out_cap) {
        utax__set_err_msg(h, "output array too small");
        return UTAX_ERR_NO_SPACE;
    }

    char sql[1024];
    UTAX_STRNCPY(sql, sizeof(sql),
        "SELECT "
        " lot_id, broker, tax_year, ticker, acq_trade_id, acq_datetime, "
        " qty_remaining, cost_per_share_eur, acq_commission_eur, "
        " last_price_update_date, last_updated_stock_price, current_lot_value_eur, "
        " country "
        "FROM fifo_snapshot WHERE 1=1"
    );

    rc = utax__build_where_plain(sql, sizeof(sql), f);
    if (rc != UTAX_OK) return rc;

    if (!UTAX_STRCAT(sql, sizeof(sql), " ORDER BY acq_datetime ASC, lot_id ASC")) return UTAX_ERR_INVALID_ARG;

    int want_pagination = (f && (f->has_limit || f->has_offset)) ? 1 : 0;
    if (want_pagination) {
        if (!UTAX_STRCAT(sql, sizeof(sql), " LIMIT ? OFFSET ?")) return UTAX_ERR_INVALID_ARG;
    }

    if (!UTAX_STRCAT(sql, sizeof(sql), ";")) return UTAX_ERR_INVALID_ARG;

    sqlite3_stmt *st = NULL;
    rc = utax__prep(h, &st, sql);
    if (rc != UTAX_OK) return rc;

    int idx = 1;
    rc = utax__bind_filters(h, st, f, &idx);
    if (rc != UTAX_OK) { sqlite3_finalize(st); return rc; }

    rc = utax__bind_pagination(h, st, f, &idx);
    if (rc != UTAX_OK) { sqlite3_finalize(st); return rc; }

    size_t i = 0;
    int s = SQLITE_OK;

    while ((s = sqlite3_step(st)) == SQLITE_ROW) {
        utax_fifo_snapshot_row *r = &out_rows[i++];

        r->lot_id = (long long)sqlite3_column_int64(st, 0);
        utax__col_text(st, 1, r->broker, sizeof(r->broker));
        r->tax_year = sqlite3_column_int(st, 2);
        utax__col_text(st, 3, r->ticker, sizeof(r->ticker));
        r->acq_trade_id = (long long)sqlite3_column_int64(st, 4);
        utax__col_text(st, 5, r->acq_datetime, sizeof(r->acq_datetime));

        r->qty_remaining = sqlite3_column_double(st, 6);
        r->cost_per_share_eur = sqlite3_column_double(st, 7);
        r->acq_commission_eur = sqlite3_column_double(st, 8);
        utax__col_text(st, 9, r->last_price_update_date, sizeof(r->last_price_update_date));
        r->last_updated_stock_price = sqlite3_column_double(st, 10);
        r->current_lot_value_eur = sqlite3_column_double(st, 11);
        utax__col_text(st, 12, r->country, sizeof(r->country));
    }

    sqlite3_finalize(st);

    if (s != SQLITE_DONE) return utax__set_err_sqlite(h, s);

    *out_count = i;
    return UTAX_OK;
}

utax_rc utax_fifo_snapshot_update_price_by_lot_id(utax_db_t *db,
                                                  long long lot_id,
                                                  const char *date_yyyy_mm_dd)
{
    if (!db || !date_yyyy_mm_dd) return UTAX_ERR_INVALID_ARG;

    struct utax_db *h = (struct utax_db *)db;
    sqlite3_stmt *sel = NULL;
    sqlite3_stmt *upd = NULL;
    utax_market_quote q;
    char ticker[UTAX_TICKER_MAX];
    utax_rc rc = UTAX_OK;
    int s = SQLITE_OK;

    ticker[0] = '\0';

    rc = utax__prep(h, &sel, "SELECT ticker FROM fifo_snapshot WHERE lot_id=?1;");
    if (rc != UTAX_OK) return rc;

    sqlite3_bind_int64(sel, 1, (sqlite3_int64)lot_id);
    s = sqlite3_step(sel);
    if (s == SQLITE_ROW) {
        utax__col_text(sel, 0, ticker, sizeof(ticker));
    } else if (s == SQLITE_DONE) {
        sqlite3_finalize(sel);
        utax__set_err_msg(h, "fifo_snapshot lot_id not found");
        return UTAX_ERR_NOT_FOUND;
    } else {
        sqlite3_finalize(sel);
        return utax__set_err_sqlite(h, s);
    }
    sqlite3_finalize(sel);

    rc = utax_market_data_lookup_yahoo_date(ticker, date_yyyy_mm_dd, 0, &q);
    if (rc != UTAX_OK) return rc;

    rc = utax__prep(h, &upd,
                    "UPDATE fifo_snapshot "
                    "SET last_price_update_date=?1, last_updated_stock_price=?2 "
                    "WHERE lot_id=?3;");
    if (rc != UTAX_OK) return rc;

    rc = utax__update_price_by_lot_stmt(h, upd, lot_id, date_yyyy_mm_dd, q.close_price);
    sqlite3_finalize(upd);
    return rc;
}

utax_rc utax_fifo_snapshot_update_prices_by_ticker(utax_db_t *db,
                                                   const char *ticker,
                                                   const char *date_yyyy_mm_dd,
                                                   size_t *out_updated,
                                                   size_t *out_unavailable)
{
    if (!db || !ticker || !date_yyyy_mm_dd) return UTAX_ERR_INVALID_ARG;

    if (out_updated) *out_updated = 0;
    if (out_unavailable) *out_unavailable = 0;

    struct utax_db *h = (struct utax_db *)db;
    sqlite3_stmt *count_st = NULL;
    sqlite3_stmt *upd = NULL;
    utax_market_quote q;
    utax_rc rc = UTAX_OK;
    long long matched = 0;

    rc = utax__prep(h, &count_st, "SELECT COUNT(*) FROM fifo_snapshot WHERE ticker=?1;");
    if (rc != UTAX_OK) return rc;
    (void)utax__bind_text(count_st, 1, ticker);

    {
        int s = sqlite3_step(count_st);
        if (s != SQLITE_ROW) {
            sqlite3_finalize(count_st);
            return utax__set_err_sqlite(h, s);
        }
        matched = (long long)sqlite3_column_int64(count_st, 0);
    }
    sqlite3_finalize(count_st);

    if (matched <= 0) return UTAX_OK;

    rc = utax_market_data_lookup_yahoo_date(ticker, date_yyyy_mm_dd, 0, &q);
    if (utax__market_lookup_unavailable(rc)) {
        if (out_unavailable) *out_unavailable = (size_t)matched;
        return UTAX_OK;
    }

    rc = utax__prep(h, &upd,
                    "UPDATE fifo_snapshot "
                    "SET last_price_update_date=?1, last_updated_stock_price=?2 "
                    "WHERE ticker=?3;");
    if (rc != UTAX_OK) return rc;

    (void)utax__bind_text(upd, 1, date_yyyy_mm_dd);
    sqlite3_bind_double(upd, 2, q.close_price);
    (void)utax__bind_text(upd, 3, ticker);

    {
        int s = sqlite3_step(upd);
        if (s != SQLITE_DONE) {
            sqlite3_finalize(upd);
            return utax__set_err_sqlite(h, s);
        }
    }

    if (out_updated) *out_updated = (size_t)sqlite3_changes(h->db);
    sqlite3_finalize(upd);
    return UTAX_OK;
}

utax_rc utax_fifo_snapshot_update_prices_paged(utax_db_t *db,
                                               const char *date_yyyy_mm_dd,
                                               const utax_fifo_snapshot_filter *f,
                                               size_t *out_processed,
                                               size_t *out_updated,
                                               size_t *out_unavailable)
{
    if (!db || !date_yyyy_mm_dd) return UTAX_ERR_INVALID_ARG;

    if (out_processed) *out_processed = 0;
    if (out_updated) *out_updated = 0;
    if (out_unavailable) *out_unavailable = 0;

    struct utax_db *h = (struct utax_db *)db;
    long long need_ll = 0;
    size_t need = 0;
    utax_fifo_snapshot_row *rows = NULL;
    size_t got = 0;
    size_t req = 0;
    size_t updated = 0;
    size_t unavailable = 0;
    sqlite3_stmt *upd = NULL;
    utax_rc rc = UTAX_OK;

    rc = utax_fifo_snapshot_count_page(db, f, &need_ll);
    if (rc != UTAX_OK) return rc;

    if (need_ll <= 0) return UTAX_OK;
    need = (size_t)need_ll;

    rows = (utax_fifo_snapshot_row *)calloc(need, sizeof(*rows));
    if (!rows) return UTAX_ERR_NOMEM;

    rc = utax_fifo_snapshot_get_filtered(db, f, rows, need, &got, &req);
    if (rc != UTAX_OK) {
        free(rows);
        return rc;
    }

    rc = utax__prep(h, &upd,
                    "UPDATE fifo_snapshot "
                    "SET last_price_update_date=?1, last_updated_stock_price=?2 "
                    "WHERE lot_id=?3;");
    if (rc != UTAX_OK) {
        free(rows);
        return rc;
    }

    for (size_t i = 0; i < got; ++i) {
        utax_market_quote q;
        utax_rc lrc = utax_market_data_lookup_yahoo_date(rows[i].ticker, date_yyyy_mm_dd, 0, &q);

        if (utax__market_lookup_unavailable(lrc)) {
            unavailable++;
            continue;
        }

        rc = utax__update_price_by_lot_stmt(h, upd, rows[i].lot_id, date_yyyy_mm_dd, q.close_price);
        if (rc == UTAX_ERR_NOT_FOUND) {
            unavailable++;
            continue;
        }
        if (rc != UTAX_OK) {
            sqlite3_finalize(upd);
            free(rows);
            return rc;
        }

        updated++;
    }

    sqlite3_finalize(upd);
    free(rows);

    if (out_processed) *out_processed = got;
    if (out_updated) *out_updated = updated;
    if (out_unavailable) *out_unavailable = unavailable;

    (void)req;
    return UTAX_OK;
}
