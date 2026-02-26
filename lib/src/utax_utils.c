#include "utax_utils.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct utax_text_builder {
    char *buf;
    size_t len;
    size_t cap;
} utax_text_builder;

static utax_rc utax__tb_init(utax_text_builder *tb) {
    if (!tb) return UTAX_ERR_INVALID_ARG;
    tb->cap = 1024;
    tb->len = 0;
    tb->buf = (char *)calloc(tb->cap, 1);
    if (!tb->buf) return UTAX_ERR_NOMEM;
    return UTAX_OK;
}

static void utax__tb_free(utax_text_builder *tb) {
    if (!tb) return;
    free(tb->buf);
    tb->buf = NULL;
    tb->len = 0;
    tb->cap = 0;
}

static utax_rc utax__tb_ensure(utax_text_builder *tb, size_t add_len) {
    if (!tb || !tb->buf) return UTAX_ERR_INVALID_ARG;
    if (add_len > (SIZE_MAX - tb->len - 1)) return UTAX_ERR_OVERFLOW;

    size_t needed = tb->len + add_len + 1;
    if (needed <= tb->cap) return UTAX_OK;

    size_t next_cap = tb->cap;
    while (next_cap < needed) {
        if (next_cap > (SIZE_MAX / 2)) {
            next_cap = needed;
            break;
        }
        next_cap *= 2;
    }

    char *next = (char *)realloc(tb->buf, next_cap);
    if (!next) return UTAX_ERR_NOMEM;
    tb->buf = next;
    tb->cap = next_cap;
    return UTAX_OK;
}

static utax_rc utax__tb_appendf(utax_text_builder *tb, const char *fmt, ...) {
    if (!tb || !fmt) return UTAX_ERR_INVALID_ARG;

    for (;;) {
        size_t avail = tb->cap - tb->len;
        va_list args;
        va_start(args, fmt);
        int written = vsnprintf(tb->buf + tb->len, avail, fmt, args);
        va_end(args);

        if (written < 0) {
            utax_rc rc = utax__tb_ensure(tb, tb->cap);
            if (rc != UTAX_OK) return rc;
            continue;
        }

        if ((size_t)written < avail) {
            tb->len += (size_t)written;
            return UTAX_OK;
        }

        utax_rc rc = utax__tb_ensure(tb, (size_t)written);
        if (rc != UTAX_OK) return rc;
    }
}

UTAX_API void utax_utils_format_month_year(const char *iso_dt, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';

    if (!iso_dt || strlen(iso_dt) < 7) return;
    (void)snprintf(out, out_size, "%.2s/%.4s", iso_dt + 5, iso_dt);
}

UTAX_API utax_rc utax_utils_build_summary_report(
    const utax_fifo_realized_row *trade_rows,
    size_t trade_count,
    const utax_dividends_country_total_row *dividend_rows,
    size_t dividend_count,
    char **out_text
) {
    if (!out_text) return UTAX_ERR_INVALID_ARG;
    *out_text = NULL;

    utax_text_builder tb;
    utax_rc rc = utax__tb_init(&tb);
    if (rc != UTAX_OK) return rc;

    double total_sell = 0.0;
    double total_buy = 0.0;
    double total_cost = 0.0;
    double total_gains = 0.0;
    double total_div_gross = 0.0;
    double total_div_taxes = 0.0;
    double total_div_total = 0.0;

    rc = utax__tb_appendf(
        &tb,
        "TRADES SUMMARY\n"
        "Ticker  Country  Qty matched   Sell date  Sell value      Bought date  Bought value     Other cost      Gains\n"
    );
    if (rc != UTAX_OK) goto fail;

    for (size_t i = 0; i < trade_count; ++i) {
        const utax_fifo_realized_row *r = &trade_rows[i];
        char sell_my[16];
        char buy_my[16];
        utax_utils_format_month_year(r->sell_datetime, sell_my, sizeof(sell_my));
        utax_utils_format_month_year(r->buy_datetime, buy_my, sizeof(buy_my));

        double gains = r->sale_value_eur - r->acquisition_value_eur - r->costs_eur;
        total_sell += r->sale_value_eur;
        total_buy += r->acquisition_value_eur;
        total_cost += r->costs_eur;
        total_gains += gains;

        rc = utax__tb_appendf(
            &tb,
            "%-7s %-8s %11.6f  %-10s %13.2f  %-11s %13.2f  %11.2f  %11.2f\n",
            r->ticker,
            r->country,
            r->qty_matched,
            sell_my,
            r->sale_value_eur,
            buy_my,
            r->acquisition_value_eur,
            r->costs_eur,
            gains
        );
        if (rc != UTAX_OK) goto fail;
    }

    rc = utax__tb_appendf(
        &tb,
        "TOTALS                                 %13.2f               %13.2f  %11.2f  %11.2f\n\n",
        total_sell,
        total_buy,
        total_cost,
        total_gains
    );
    if (rc != UTAX_OK) goto fail;

    rc = utax__tb_appendf(
        &tb,
        "DIVIDENDS SUMMARY\n"
        "Country  Gross          Taxes          Total\n"
    );
    if (rc != UTAX_OK) goto fail;

    for (size_t i = 0; i < dividend_count; ++i) {
        const utax_dividends_country_total_row *d = &dividend_rows[i];
        total_div_gross += d->gross_amount_eur;
        total_div_taxes += d->taxes_eur;
        total_div_total += d->total_eur;

        rc = utax__tb_appendf(
            &tb,
            "%-7s %13.2f  %13.2f  %13.2f\n",
            d->country,
            d->gross_amount_eur,
            d->taxes_eur,
            d->total_eur
        );
        if (rc != UTAX_OK) goto fail;
    }

    rc = utax__tb_appendf(
        &tb,
        "TOTALS  %13.2f  %13.2f  %13.2f\n",
        total_div_gross,
        total_div_taxes,
        total_div_total
    );
    if (rc != UTAX_OK) goto fail;

    *out_text = tb.buf;
    return UTAX_OK;

fail:
    utax__tb_free(&tb);
    return rc;
}

UTAX_API utax_rc utax_utils_write_text_file(const char *path, const char *text) {
    if (!path || !text) return UTAX_ERR_INVALID_ARG;

    FILE *f = fopen(path, "wb");
    if (!f) return UTAX_ERR_IO_OPEN;

    size_t len = strlen(text);
    size_t written = fwrite(text, 1, len, f);
    if (written != len) {
        fclose(f);
        return UTAX_ERR_IO_READ;
    }

    fclose(f);
    return UTAX_OK;
}

UTAX_API void utax_utils_free_text(char **inout_text) {
    if (!inout_text) return;
    free(*inout_text);
    *inout_text = NULL;
}
