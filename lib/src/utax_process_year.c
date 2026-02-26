#include "utax_process_year.h"

#include "utax_corporate_actions.h"
#include "utax_dividends.h"
#include "utax_fifo_realized.h"
#include "utax_fifo_snapshot.h"
#include "utax_fifo_snapshot_action_applied.h"
#include "utax_schema.h"
#include "utax_trades.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define UTAX_EPS 1e-12
#define UTAX_QTY_EPS 1e-6

static int utax__is_zero(double v) {
    return (v > -UTAX_EPS && v < UTAX_EPS);
}

static int utax__is_qty_zero(double v) {
    return (v > -UTAX_QTY_EPS && v < UTAX_QTY_EPS);
}

static void utax__copy_text(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t n = strlen(src);
    if (n >= dst_sz) n = dst_sz - 1;
    if (n > 0) memcpy(dst, src, n);
    dst[n] = '\0';
}

static int utax__parse_broker_transfer(const char *broker_field,
                                       char *out_src,
                                       size_t out_src_sz,
                                       char *out_dst,
                                       size_t out_dst_sz)
{
    if (!broker_field || !out_src || !out_dst || out_src_sz == 0 || out_dst_sz == 0) return 0;

    const char *arrow = strstr(broker_field, "->");
    if (!arrow) return 0;

    const char *src_b = broker_field;
    const char *src_e = arrow;
    while (src_b < src_e && isspace((unsigned char)*src_b)) src_b++;
    while (src_e > src_b && isspace((unsigned char)src_e[-1])) src_e--;

    const char *dst_b = arrow + 2;
    const char *dst_e = broker_field + strlen(broker_field);
    while (dst_b < dst_e && isspace((unsigned char)*dst_b)) dst_b++;
    while (dst_e > dst_b && isspace((unsigned char)dst_e[-1])) dst_e--;

    if (src_b >= src_e || dst_b >= dst_e) return 0;

    size_t src_n = (size_t)(src_e - src_b);
    if (src_n >= out_src_sz) src_n = out_src_sz - 1;
    memcpy(out_src, src_b, src_n);
    out_src[src_n] = '\0';

    size_t dst_n = (size_t)(dst_e - dst_b);
    if (dst_n >= out_dst_sz) dst_n = out_dst_sz - 1;
    memcpy(out_dst, dst_b, dst_n);
    out_dst[dst_n] = '\0';

    return (out_src[0] != '\0' && out_dst[0] != '\0');
}

static void utax__append_unique_broker(char (*brokers)[UTAX_BROKER_MAX],
                                       size_t cap,
                                       size_t *inout_count,
                                       const char *broker)
{
    if (!brokers || !inout_count || !broker || broker[0] == '\0') return;
    if (*inout_count >= cap) return;

    for (size_t i = 0; i < *inout_count; ++i) {
        if (strcmp(brokers[i], broker) == 0) return;
    }

    utax__copy_text(brokers[*inout_count], sizeof(brokers[*inout_count]), broker);
    (*inout_count)++;
}

static utax_rc utax__collect_actions_for_year(utax_db_t *db,
                                              uint16_t year,
                                              utax_corporate_actions_row **out_rows,
                                              size_t *out_count)
{
    if (!db || !out_rows || !out_count) return UTAX_ERR_INVALID_ARG;
    *out_rows = NULL;
    *out_count = 0;

    utax_corporate_actions_filter f;
    memset(&f, 0, sizeof(f));
    f.has_year = 1;
    f.year = (int)year;
    f.year_mode = UTAX_YEAR_EXACT;

    long long needed_ll = 0;
    utax_rc rc = utax_corporate_actions_count_filtered(db, &f, &needed_ll);
    if (rc != UTAX_OK || needed_ll <= 0) return rc;

    size_t needed = (size_t)needed_ll;
    utax_corporate_actions_row *rows = (utax_corporate_actions_row *)calloc(needed, sizeof(*rows));
    if (!rows) return UTAX_ERR_NOMEM;

    size_t got = 0;
    size_t req = 0;
    rc = utax_corporate_actions_get_filtered(db, &f, rows, needed, &got, &req);
    if (rc != UTAX_OK) {
        free(rows);
        return rc;
    }

    *out_rows = rows;
    *out_count = got;
    return UTAX_OK;
}

UTAX_API void process_year_free_realized_rows(
    utax_fifo_realized_row **inout_rows,
    size_t *inout_count
) {
    if (!inout_rows || !inout_count) return;
    free(*inout_rows);
    *inout_rows = NULL;
    *inout_count = 0;
}

static utax_rc utax__collect_brokers_for_year(utax_db_t *db,
                                               uint16_t year,
                                               char (**out_brokers)[UTAX_BROKER_MAX],
                                               size_t *out_count)
{
    if (!db || !out_brokers || !out_count) return UTAX_ERR_INVALID_ARG;
    *out_brokers = NULL;
    *out_count = 0;

    utax_trades_filter f;
    memset(&f, 0, sizeof(f));
    f.has_year = 1;
    f.year = (int)year;
    f.year_mode = UTAX_YEAR_EXACT;

    long long trades_needed_ll = 0;
    utax_rc rc = utax_trades_count_filtered(db, &f, &trades_needed_ll);
    if (rc != UTAX_OK) return rc;

    size_t trades_needed = (trades_needed_ll <= 0) ? 0 : (size_t)trades_needed_ll;
    utax_trades_row *rows = NULL;
    size_t got = 0;
    if (trades_needed > 0) {
        rows = (utax_trades_row *)calloc(trades_needed, sizeof(*rows));
        if (!rows) return UTAX_ERR_NOMEM;

        size_t req = 0;
        rc = utax_trades_get_filtered(db, &f, rows, trades_needed, &got, &req);
        if (rc != UTAX_OK) {
            free(rows);
            return rc;
        }
    }

    utax_corporate_actions_row *actions = NULL;
    size_t action_count = 0;
    rc = utax__collect_actions_for_year(db, year, &actions, &action_count);
    if (rc != UTAX_OK) {
        free(rows);
        return rc;
    }

    size_t cap = got + (action_count * 2);
    if (cap == 0) {
        free(rows);
        free(actions);
        return UTAX_OK;
    }

    char (*brokers)[UTAX_BROKER_MAX] = (char (*)[UTAX_BROKER_MAX])calloc(cap, sizeof(*brokers));
    if (!brokers) {
        free(rows);
        free(actions);
        return UTAX_ERR_NOMEM;
    }

    size_t broker_count = 0;
    for (size_t i = 0; i < action_count; ++i) {
        char src[UTAX_BROKER_MAX];
        char dst[UTAX_BROKER_MAX];
        if (utax__parse_broker_transfer(actions[i].broker, src, sizeof(src), dst, sizeof(dst))) {
            utax__append_unique_broker(brokers, cap, &broker_count, src);
            utax__append_unique_broker(brokers, cap, &broker_count, dst);
        } else {
            utax__append_unique_broker(brokers, cap, &broker_count, actions[i].broker);
        }
    }

    for (size_t i = 0; i < got; ++i) {
        if (!(strcmp(rows[i].type, "BUY") == 0 || strcmp(rows[i].type, "SELL") == 0)) continue;
        utax__append_unique_broker(brokers, cap, &broker_count, rows[i].broker);
    }

    free(rows);
    free(actions);

    *out_brokers = brokers;
    *out_count = broker_count;
    return UTAX_OK;
}

static utax_rc utax__collect_trades_for_broker_type_year(utax_db_t *db,
                                                          const char *broker,
                                                          const char *type,
                                                          uint16_t year,
                                                          utax_trades_row **out_rows,
                                                          size_t *out_count)
{
    if (!db || !broker || !type || !out_rows || !out_count) return UTAX_ERR_INVALID_ARG;
    *out_rows = NULL;
    *out_count = 0;

    utax_trades_filter f;
    memset(&f, 0, sizeof(f));
    f.has_year = 1;
    f.year = (int)year;
    f.year_mode = UTAX_YEAR_EXACT;
    f.has_broker = 1;
    utax__copy_text(f.broker, sizeof(f.broker), broker);
    f.has_type = 1;
    utax__copy_text(f.type, sizeof(f.type), type);

    long long needed_ll = 0;
    utax_rc rc = utax_trades_count_filtered(db, &f, &needed_ll);
    if (rc != UTAX_OK || needed_ll <= 0) return rc;

    size_t needed = (size_t)needed_ll;
    utax_trades_row *rows = (utax_trades_row *)calloc(needed, sizeof(*rows));
    if (!rows) return UTAX_ERR_NOMEM;

    size_t got = 0;
    size_t req = 0;
    rc = utax_trades_get_filtered(db, &f, rows, needed, &got, &req);
    if (rc != UTAX_OK) {
        free(rows);
        return rc;
    }

    *out_rows = rows;
    *out_count = got;
    return UTAX_OK;
}

static utax_rc utax__collect_snapshot_for_broker_up_to_year(utax_db_t *db,
                                                             const char *broker,
                                                             int max_year,
                                                             utax_fifo_snapshot_row **out_rows,
                                                             size_t *out_count)
{
    if (!db || !broker || !out_rows || !out_count) return UTAX_ERR_INVALID_ARG;
    *out_rows = NULL;
    *out_count = 0;

    if (max_year < 0) return UTAX_OK;

    utax_fifo_snapshot_filter f;
    memset(&f, 0, sizeof(f));
    f.has_year = 1;
    f.year = max_year;
    f.year_mode = UTAX_YEAR_UP_TO;
    f.has_broker = 1;
    utax__copy_text(f.broker, sizeof(f.broker), broker);

    long long needed_ll = 0;
    utax_rc rc = utax_fifo_snapshot_count_filtered(db, &f, &needed_ll);
    if (rc != UTAX_OK || needed_ll <= 0) return rc;

    size_t needed = (size_t)needed_ll;
    utax_fifo_snapshot_row *rows = (utax_fifo_snapshot_row *)calloc(needed, sizeof(*rows));
    if (!rows) return UTAX_ERR_NOMEM;

    size_t got = 0;
    size_t req = 0;
    rc = utax_fifo_snapshot_get_filtered(db, &f, rows, needed, &got, &req);
    if (rc != UTAX_OK) {
        free(rows);
        return rc;
    }

    *out_rows = rows;
    *out_count = got;
    return UTAX_OK;
}

static utax_rc utax__collect_snapshot_for_broker_year(utax_db_t *db,
                                                       const char *broker,
                                                       int year,
                                                       utax_fifo_snapshot_row **out_rows,
                                                       size_t *out_count)
{
    if (!db || !broker || !out_rows || !out_count) return UTAX_ERR_INVALID_ARG;
    *out_rows = NULL;
    *out_count = 0;

    utax_fifo_snapshot_filter f;
    memset(&f, 0, sizeof(f));
    f.has_year = 1;
    f.year = year;
    f.year_mode = UTAX_YEAR_EXACT;
    f.has_broker = 1;
    utax__copy_text(f.broker, sizeof(f.broker), broker);

    long long needed_ll = 0;
    utax_rc rc = utax_fifo_snapshot_count_filtered(db, &f, &needed_ll);
    if (rc != UTAX_OK || needed_ll <= 0) return rc;

    size_t needed = (size_t)needed_ll;
    utax_fifo_snapshot_row *rows = (utax_fifo_snapshot_row *)calloc(needed, sizeof(*rows));
    if (!rows) return UTAX_ERR_NOMEM;

    size_t got = 0;
    size_t req = 0;
    rc = utax_fifo_snapshot_get_filtered(db, &f, rows, needed, &got, &req);
    if (rc != UTAX_OK) {
        free(rows);
        return rc;
    }

    *out_rows = rows;
    *out_count = got;
    return UTAX_OK;
}

static utax_rc utax__collect_actions_for_broker_year(utax_db_t *db,
                                                      const char *broker,
                                                      uint16_t year,
                                                      utax_corporate_actions_row **out_rows,
                                                      size_t *out_count)
{
    if (!db || !broker || !out_rows || !out_count) return UTAX_ERR_INVALID_ARG;
    *out_rows = NULL;
    *out_count = 0;

    utax_corporate_actions_row *all_rows = NULL;
    size_t all_count = 0;
    utax_rc rc = utax__collect_actions_for_year(db, year, &all_rows, &all_count);
    if (rc != UTAX_OK || all_count == 0) return rc;

    utax_corporate_actions_row *rows = (utax_corporate_actions_row *)calloc(all_count, sizeof(*rows));
    if (!rows) {
        free(all_rows);
        return UTAX_ERR_NOMEM;
    }

    size_t count = 0;
    for (size_t i = 0; i < all_count; ++i) {
        char src[UTAX_BROKER_MAX];
        char dst[UTAX_BROKER_MAX];
        int is_transfer = utax__parse_broker_transfer(all_rows[i].broker, src, sizeof(src), dst, sizeof(dst));
        if ((!is_transfer && strcmp(all_rows[i].broker, broker) == 0) ||
            (is_transfer && strcmp(src, broker) == 0)) {
            rows[count++] = all_rows[i];
        }
    }

    free(all_rows);

    if (count == 0) {
        free(rows);
        return UTAX_OK;
    }

    *out_rows = rows;
    *out_count = count;
    return UTAX_OK;
}

static utax_rc utax__action_already_applied_for_lot(utax_db_t *db,
                                                     long long lot_id,
                                                     long long action_id,
                                                     int *out_applied)
{
    if (!db || !out_applied) return UTAX_ERR_INVALID_ARG;
    *out_applied = 0;

    utax_fifo_snapshot_action_applied_filter f;
    memset(&f, 0, sizeof(f));
    f.has_lot_id = 1;
    f.lot_id = lot_id;
    f.has_action_id = 1;
    f.action_id = action_id;

    long long count = 0;
    utax_rc rc = utax_fifo_snapshot_action_applied_count_filtered(db, &f, &count);
    if (rc != UTAX_OK) return rc;

    *out_applied = (count > 0) ? 1 : 0;
    return UTAX_OK;
}

static utax_rc utax__mark_action_applied_for_lot(utax_db_t *db, long long lot_id, long long action_id) {
    utax_fifo_snapshot_action_applied_row row;
    row.lot_id = lot_id;
    row.action_id = action_id;
    return utax_fifo_snapshot_action_applied_insert(db, &row);
}

static utax_rc utax__collect_realized_for_broker_year(utax_db_t *db,
                                                       const char *broker,
                                                       int year,
                                                       utax_fifo_realized_row **out_rows,
                                                       size_t *out_count)
{
    if (!db || !broker || !out_rows || !out_count) return UTAX_ERR_INVALID_ARG;
    *out_rows = NULL;
    *out_count = 0;

    utax_fifo_realized_filter f;
    memset(&f, 0, sizeof(f));
    f.has_year = 1;
    f.year = year;
    f.year_mode = UTAX_YEAR_EXACT;
    f.has_broker = 1;
    utax__copy_text(f.broker, sizeof(f.broker), broker);

    long long needed_ll = 0;
    utax_rc rc = utax_fifo_realized_count_filtered(db, &f, &needed_ll);
    if (rc != UTAX_OK || needed_ll <= 0) return rc;

    size_t needed = (size_t)needed_ll;
    utax_fifo_realized_row *rows = (utax_fifo_realized_row *)calloc(needed, sizeof(*rows));
    if (!rows) return UTAX_ERR_NOMEM;

    size_t got = 0;
    size_t req = 0;
    rc = utax_fifo_realized_get_filtered(db, &f, rows, needed, &got, &req);
    if (rc != UTAX_OK) {
        free(rows);
        return rc;
    }

    *out_rows = rows;
    *out_count = got;
    return UTAX_OK;
}

static utax_rc utax__convert_buys_to_snapshots(const utax_trades_row *buy_rows,
                                                size_t buy_count,
                                                uint16_t year,
                                                utax_fifo_snapshot_row **out_rows,
                                                size_t *out_count)
{
    if (!out_rows || !out_count) return UTAX_ERR_INVALID_ARG;
    *out_rows = NULL;
    *out_count = 0;
    if (!buy_rows || buy_count == 0) return UTAX_OK;

    utax_fifo_snapshot_row *rows = (utax_fifo_snapshot_row *)calloc(buy_count, sizeof(*rows));
    if (!rows) return UTAX_ERR_NOMEM;

    for (size_t i = 0; i < buy_count; ++i) {
        const utax_trades_row *b = &buy_rows[i];
        utax_fifo_snapshot_row *s = &rows[i];

        s->lot_id = 0;
        s->acq_trade_id = b->id;
        s->qty_remaining = b->quantity;

        double conv = (b->conversion_rate_eur > 0.0) ? b->conversion_rate_eur : 1.0;
        s->cost_per_share_eur = b->price_per_share / conv;
        s->acq_commission_eur = b->commission / conv;

        s->tax_year = (int)year;
        utax__copy_text(s->broker, sizeof(s->broker), b->broker);
        utax__copy_text(s->ticker, sizeof(s->ticker), b->ticker);
        utax__copy_text(s->acq_datetime, sizeof(s->acq_datetime), b->trade_datetime);
        utax__copy_text(s->country, sizeof(s->country), b->country);
    }

    *out_rows = rows;
    *out_count = buy_count;
    return UTAX_OK;
}

static int utax__lot_exists_before_or_on_action(const utax_fifo_snapshot_row *lot,
                                                 const utax_corporate_actions_row *action)
{
    if (!lot || !action) return 0;
    if (strlen(lot->acq_datetime) < 10 || strlen(action->action_date) < 10) return 0;
    return strncmp(lot->acq_datetime, action->action_date, 10) <= 0;
}

static utax_rc utax__apply_actions_to_snapshots(utax_db_t *db,
                                                utax_fifo_snapshot_row *rows,
                                                size_t row_count,
                                                const utax_corporate_actions_row *actions,
                                                size_t action_count,
                                                int track_idempotency)
{
    if (!rows || !actions) return UTAX_OK;

    for (size_t a = 0; a < action_count; ++a) {
        const utax_corporate_actions_row *act = &actions[a];
        char transfer_src[UTAX_BROKER_MAX];
        char transfer_dst[UTAX_BROKER_MAX];
        int is_transfer = utax__parse_broker_transfer(act->broker,
                                                      transfer_src, sizeof(transfer_src),
                                                      transfer_dst, sizeof(transfer_dst));
        const char *action_broker = is_transfer ? transfer_src : act->broker;

        double ratio = (act->ratio > 0.0) ? act->ratio : (act->to_qty / act->from_qty);
        if (!is_transfer && ratio <= 0.0) continue;

        for (size_t i = 0; i < row_count; ++i) {
            utax_fifo_snapshot_row *lot = &rows[i];
            if (lot->qty_remaining <= UTAX_QTY_EPS) continue;
            if (strcmp(lot->broker, action_broker) != 0) continue;
            if (strcmp(lot->ticker, act->from_ticker) != 0) continue;
            if (!utax__lot_exists_before_or_on_action(lot, act)) continue;

            if (track_idempotency && lot->lot_id > 0) {
                int already_applied = 0;
                utax_rc rc = utax__action_already_applied_for_lot(db, lot->lot_id, act->action_id, &already_applied);
                if (rc != UTAX_OK) return rc;
                if (already_applied) continue;
            }

            if (is_transfer) {
                utax__copy_text(lot->broker, sizeof(lot->broker), transfer_dst);
            } else {
                lot->qty_remaining *= ratio;
                lot->cost_per_share_eur /= ratio;

                if (strcmp(act->action_type, "SPLIT") != 0 && act->to_ticker[0] != '\0') {
                    utax__copy_text(lot->ticker, sizeof(lot->ticker), act->to_ticker);
                }
            }

            if (track_idempotency && lot->lot_id > 0) {
                utax_rc rc = utax__mark_action_applied_for_lot(db, lot->lot_id, act->action_id);
                if (rc != UTAX_OK) return rc;
            }
        }
    }

    return UTAX_OK;
}

static utax_rc utax__append_realized_row(utax_fifo_realized_row **rows,
                                         size_t *count,
                                         size_t *cap,
                                         const utax_fifo_realized_row *item)
{
    if (!rows || !count || !cap || !item) return UTAX_ERR_INVALID_ARG;

    if (*count == *cap) {
        size_t next_cap = (*cap == 0) ? 16 : (*cap * 2);
        utax_fifo_realized_row *next = (utax_fifo_realized_row *)realloc(*rows, next_cap * sizeof(*next));
        if (!next) return UTAX_ERR_NOMEM;
        *rows = next;
        *cap = next_cap;
    }

    (*rows)[*count] = *item;
    (*count)++;
    return UTAX_OK;
}

static utax_rc utax__delete_realized_for_broker_year(utax_db_t *db, const char *broker, int year) {
    utax_fifo_realized_row *rows = NULL;
    size_t count = 0;
    utax_rc rc = utax__collect_realized_for_broker_year(db, broker, year, &rows, &count);
    if (rc != UTAX_OK) return rc;

    for (size_t i = 0; i < count; ++i) {
        rc = utax_fifo_realized_delete_by_id(db, rows[i].realized_id);
        if (rc != UTAX_OK) {
            free(rows);
            return rc;
        }
    }

    free(rows);
    return UTAX_OK;
}

static utax_rc utax__delete_snapshot_for_broker_year(utax_db_t *db, const char *broker, int year) {
    utax_fifo_snapshot_row *rows = NULL;
    size_t count = 0;
    utax_rc rc = utax__collect_snapshot_for_broker_year(db, broker, year, &rows, &count);
    if (rc != UTAX_OK) return rc;

    long long trade_needed_ll = 0;
    rc = utax_trades_count_total(db, &trade_needed_ll);
    if (rc != UTAX_OK) {
        free(rows);
        return rc;
    }

    utax_trades_row *trade_rows = NULL;
    size_t trade_count = 0;
    if (trade_needed_ll > 0) {
        size_t trade_needed = (size_t)trade_needed_ll;
        trade_rows = (utax_trades_row *)calloc(trade_needed, sizeof(*trade_rows));
        if (!trade_rows) {
            free(rows);
            return UTAX_ERR_NOMEM;
        }

        utax_trades_filter tf;
        memset(&tf, 0, sizeof(tf));
        size_t req = 0;
        rc = utax_trades_get_filtered(db, &tf, trade_rows, trade_needed, &trade_count, &req);
        if (rc != UTAX_OK) {
            free(trade_rows);
            free(rows);
            return rc;
        }
    }

    for (size_t i = 0; i < count; ++i) {
        int delete_row = 1;
        for (size_t t = 0; t < trade_count; ++t) {
            if (trade_rows[t].id == rows[i].acq_trade_id) {
                if (strcmp(trade_rows[t].broker, broker) != 0) {
                    delete_row = 0;
                }
                break;
            }
        }
        if (!delete_row) continue;

        rc = utax_fifo_snapshot_delete_by_id(db, rows[i].lot_id);
        if (rc != UTAX_OK) {
            free(trade_rows);
            free(rows);
            return rc;
        }
    }

    free(trade_rows);
    free(rows);
    return UTAX_OK;
}

UTAX_API utax_rc process_year_trades(
    utax_db_t *db,
    uint16_t year,
    utax_fifo_realized_row **out_rows,
    size_t *out_count
) {
    if (!db) return UTAX_ERR_INVALID_ARG;
    if (out_rows && !out_count) return UTAX_ERR_INVALID_ARG;

    char (*brokers)[UTAX_BROKER_MAX] = NULL;
    size_t broker_count = 0;

    utax_fifo_realized_row *export_rows = NULL;
    size_t export_count = 0;
    size_t export_cap = 0;
    if (out_rows) {
        *out_rows = NULL;
        *out_count = 0;
    }

    utax_rc rc = utax__collect_brokers_for_year(db, year, &brokers, &broker_count);
    if (rc != UTAX_OK) return rc;

    for (size_t i = 0; i < broker_count; ++i) {
        const char *broker = brokers[i];

        utax_trades_row *buy_rows = NULL;
        size_t buy_count = 0;
        utax_trades_row *sell_rows = NULL;
        size_t sell_count = 0;
        utax_fifo_snapshot_row *snapshot_rows = NULL;
        size_t snapshot_count = 0;
        utax_corporate_actions_row *action_rows = NULL;
        size_t action_count = 0;
        utax_fifo_snapshot_row *buy_snapshot_rows = NULL;
        size_t buy_snapshot_count = 0;

        rc = utax__delete_realized_for_broker_year(db, broker, (int)year);
        if (rc != UTAX_OK) break;

        rc = utax__delete_snapshot_for_broker_year(db, broker, (int)year);
        if (rc != UTAX_OK) break;

        rc = utax__collect_trades_for_broker_type_year(db, broker, "BUY", year, &buy_rows, &buy_count);
        if (rc != UTAX_OK) goto broker_cleanup;

        rc = utax__collect_trades_for_broker_type_year(db, broker, "SELL", year, &sell_rows, &sell_count);
        if (rc != UTAX_OK) goto broker_cleanup;

        rc = utax__collect_snapshot_for_broker_up_to_year(db,
                                                          broker,
                                                          (int)year - 1,
                                                          &snapshot_rows,
                                                          &snapshot_count);
        if (rc != UTAX_OK) goto broker_cleanup;

        rc = utax__collect_actions_for_broker_year(db, broker, year, &action_rows, &action_count);
        if (rc != UTAX_OK) goto broker_cleanup;

        rc = utax__convert_buys_to_snapshots(buy_rows,
                                             buy_count,
                                             year,
                                             &buy_snapshot_rows,
                                             &buy_snapshot_count);
        if (rc != UTAX_OK) goto broker_cleanup;

        if (action_count > 0) {
            rc = utax__apply_actions_to_snapshots(db, snapshot_rows, snapshot_count, action_rows, action_count, 1);
            if (rc != UTAX_OK) goto broker_cleanup;
            rc = utax__apply_actions_to_snapshots(db, buy_snapshot_rows, buy_snapshot_count, action_rows, action_count, 0);
            if (rc != UTAX_OK) goto broker_cleanup;
        }

        utax_fifo_realized_row *realized_rows = NULL;
        size_t realized_count = 0;
        size_t realized_cap = 0;

        for (size_t s = 0; s < sell_count; ++s) {
            const utax_trades_row *sell = &sell_rows[s];
            double qty_to_match = sell->quantity;
            if (qty_to_match <= 0.0) continue;

            double sell_conv = (sell->conversion_rate_eur > 0.0) ? sell->conversion_rate_eur : 1.0;
            double sell_price_eur = sell->price_per_share / sell_conv;
            double sell_comm_eur_total = sell->commission / sell_conv;

            int match_seq = 1;

            for (size_t pass = 0; pass < 2 && qty_to_match > UTAX_QTY_EPS; ++pass) {
                utax_fifo_snapshot_row *lots = (pass == 0) ? snapshot_rows : buy_snapshot_rows;
                size_t lot_count = (pass == 0) ? snapshot_count : buy_snapshot_count;

                for (size_t l = 0; l < lot_count && qty_to_match > UTAX_QTY_EPS; ++l) {
                    utax_fifo_snapshot_row *lot = &lots[l];
                    if (lot->qty_remaining <= UTAX_QTY_EPS) continue;
                    if (strcmp(lot->ticker, sell->ticker) != 0) continue;

                    double lot_qty_before = lot->qty_remaining;
                    double matched = (lot_qty_before < qty_to_match) ? lot_qty_before : qty_to_match;
                    if (matched <= UTAX_QTY_EPS) continue;

                    int lot_depleted_after = ((lot_qty_before - matched) <= UTAX_QTY_EPS);
                    double acq_comm_alloc = 0.0;
                    if (lot_depleted_after && lot->acq_commission_eur > UTAX_EPS) {
                        acq_comm_alloc = lot->acq_commission_eur;
                    }
                    lot->acq_commission_eur -= acq_comm_alloc;
                    if (utax__is_zero(lot->acq_commission_eur) || lot->acq_commission_eur < 0.0) {
                        lot->acq_commission_eur = 0.0;
                    }

                    int sell_fully_matched_after = ((qty_to_match - matched) <= UTAX_QTY_EPS);
                    double sell_comm_alloc = 0.0;
                    if (sell_fully_matched_after && sell_comm_eur_total > UTAX_EPS) {
                        sell_comm_alloc = sell_comm_eur_total;
                    }

                    utax_fifo_realized_row rr;
                    memset(&rr, 0, sizeof(rr));
                    rr.sell_trade_id = sell->id;
                    rr.buy_trade_id = lot->acq_trade_id;
                    rr.qty_matched = matched;
                    rr.acquisition_value_eur = matched * lot->cost_per_share_eur;
                    rr.sale_value_eur = matched * sell_price_eur;
                    rr.costs_eur = acq_comm_alloc + sell_comm_alloc;
                    rr.tax_year = (int)year;
                    rr.match_seq = match_seq++;
                    utax__copy_text(rr.broker, sizeof(rr.broker), sell->broker);
                    utax__copy_text(rr.ticker, sizeof(rr.ticker), sell->ticker);
                    utax__copy_text(rr.country, sizeof(rr.country), sell->country);
                    utax__copy_text(rr.sell_datetime, sizeof(rr.sell_datetime), sell->trade_datetime);
                    utax__copy_text(rr.buy_datetime, sizeof(rr.buy_datetime), lot->acq_datetime);

                    rc = utax__append_realized_row(&realized_rows, &realized_count, &realized_cap, &rr);
                    if (rc != UTAX_OK) {
                        free(realized_rows);
                        goto broker_cleanup;
                    }

                    lot->qty_remaining -= matched;
                    if (utax__is_qty_zero(lot->qty_remaining) || lot->qty_remaining < 0.0) {
                        lot->qty_remaining = 0.0;
                    }

                    qty_to_match -= matched;
                    if (utax__is_qty_zero(qty_to_match) || qty_to_match < 0.0) {
                        qty_to_match = 0.0;
                    }
                }
            }
        }

        if (realized_count > 0) {
            size_t inserted = 0;
            rc = utax_fifo_realized_insert_many(db, realized_rows, realized_count, &inserted);
            if (rc != UTAX_OK) {
                free(realized_rows);
                goto broker_cleanup;
            }

            if (out_rows) {
                for (size_t r = 0; r < realized_count; ++r) {
                    rc = utax__append_realized_row(&export_rows, &export_count, &export_cap, &realized_rows[r]);
                    if (rc != UTAX_OK) {
                        free(realized_rows);
                        goto broker_cleanup;
                    }
                }
            }

            free(realized_rows);
        }

        for (size_t sr = 0; sr < snapshot_count; ++sr) {
            utax_fifo_snapshot_row *row = &snapshot_rows[sr];

            if (row->qty_remaining <= UTAX_QTY_EPS) {
                rc = utax_fifo_snapshot_delete_by_id(db, row->lot_id);
            } else {
                if (strcmp(row->broker, broker) == 0) {
                    row->tax_year = (int)year;
                }
                rc = utax_fifo_snapshot_update_by_id(db, row->lot_id, row);
            }

            if (rc != UTAX_OK) goto broker_cleanup;
        }

        size_t remaining_buys = 0;
        for (size_t b = 0; b < buy_snapshot_count; ++b) {
            if (buy_snapshot_rows[b].qty_remaining > UTAX_QTY_EPS) remaining_buys++;
        }

        if (remaining_buys > 0) {
            utax_fifo_snapshot_row *ins = (utax_fifo_snapshot_row *)calloc(remaining_buys, sizeof(*ins));
            if (!ins) {
                rc = UTAX_ERR_NOMEM;
                goto broker_cleanup;
            }

            size_t k = 0;
            for (size_t b = 0; b < buy_snapshot_count; ++b) {
                if (buy_snapshot_rows[b].qty_remaining > UTAX_QTY_EPS) {
                    buy_snapshot_rows[b].tax_year = (int)year;
                    ins[k++] = buy_snapshot_rows[b];
                }
            }

            size_t inserted = 0;
            rc = utax_fifo_snapshot_insert_many(db, ins, remaining_buys, &inserted);
            free(ins);
            if (rc != UTAX_OK) goto broker_cleanup;
        }

broker_cleanup:
        free(buy_rows);
        free(sell_rows);
        free(snapshot_rows);
        free(action_rows);
        free(buy_snapshot_rows);

        if (rc != UTAX_OK) break;
    }

    free(brokers);
    if (rc != UTAX_OK) {
        free(export_rows);
        return rc;
    }

    if (out_rows) {
        *out_rows = export_rows;
        *out_count = export_count;
    } else {
        free(export_rows);
    }
    return rc;
}

UTAX_API utax_rc process_year_dividends_country_totals(
    utax_db_t *db,
    uint16_t year,
    utax_dividends_country_total_row **out_rows,
    size_t *out_count
) {
    if (!db) return UTAX_ERR_INVALID_ARG;
    if (out_rows && !out_count) return UTAX_ERR_INVALID_ARG;
    if (out_rows) {
        *out_rows = NULL;
        *out_count = 0;
    }

    utax_dividends_filter f;
    memset(&f, 0, sizeof(f));
    f.has_year = 1;
    f.year = (int)year;
    f.year_mode = UTAX_YEAR_EXACT;

    long long needed_ll = 0;
    utax_rc rc = utax_dividends_count_filtered(db, &f, &needed_ll);
    if (rc != UTAX_OK) return rc;
    if (needed_ll <= 0) return UTAX_OK;

    size_t needed = (size_t)needed_ll;
    utax_dividends_row *rows = (utax_dividends_row *)calloc(needed, sizeof(*rows));
    if (!rows) return UTAX_ERR_NOMEM;

    size_t got = 0;
    size_t req = 0;
    rc = utax_dividends_get_filtered(db, &f, rows, needed, &got, &req);
    if (rc != UTAX_OK) {
        free(rows);
        return rc;
    }

    utax_dividends_country_total_row *agg = (utax_dividends_country_total_row *)calloc(got, sizeof(*agg));
    if (!agg) {
        free(rows);
        return UTAX_ERR_NOMEM;
    }

    size_t agg_n = 0;
    for (size_t i = 0; i < got; ++i) {
        const utax_dividends_row *d = &rows[i];
        double conv = (d->conversion_rate_eur > 0.0) ? d->conversion_rate_eur : 1.0;
        double gross_eur = d->total_amount / conv;
        double taxes_eur = d->tax / conv;

        size_t k = 0;
        int found = 0;
        for (; k < agg_n; ++k) {
            if (strcmp(agg[k].country, d->country) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            utax__copy_text(agg[agg_n].country, sizeof(agg[agg_n].country), d->country);
            k = agg_n++;
        }

        agg[k].gross_amount_eur += gross_eur;
        agg[k].taxes_eur += taxes_eur;
        agg[k].total_eur = agg[k].gross_amount_eur - agg[k].taxes_eur;
    }

    if (out_rows) {
        utax_dividends_country_total_row *res =
            (utax_dividends_country_total_row *)calloc(agg_n, sizeof(*res));
        if (!res) {
            free(agg);
            free(rows);
            return UTAX_ERR_NOMEM;
        }
        for (size_t i = 0; i < agg_n; ++i) res[i] = agg[i];
        *out_rows = res;
        *out_count = agg_n;
    }

    free(agg);
    free(rows);
    return UTAX_OK;
}

UTAX_API void process_year_free_dividends_country_total_rows(
    utax_dividends_country_total_row **inout_rows,
    size_t *inout_count
) {
    if (!inout_rows || !inout_count) return;
    free(*inout_rows);
    *inout_rows = NULL;
    *inout_count = 0;
}
