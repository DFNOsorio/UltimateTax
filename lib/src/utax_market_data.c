#include "utax_market_data.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#if defined(_WIN32)
  #include <process.h>
  #include <windows.h>
  #define UTAX_POPEN _popen
  #define UTAX_PCLOSE _pclose
#else
  #include <sys/wait.h>
  #include <unistd.h>
  #define UTAX_POPEN popen
  #define UTAX_PCLOSE pclose
#endif

enum {
    UTAX_MARKET_URL_MAX = 1024,
    UTAX_MARKET_CMD_MAX = 1200,
    UTAX_MARKET_JSON_MAX = 262144,
    UTAX_MARKET_CALL_THROTTLE_MS = 200,
    UTAX_MARKET_FX_CALL_THROTTLE_MS = 150
};

static utax_rc utax__fetch_url_text(const char *url, char *out_buf, size_t out_cap, size_t *out_len);

static void utax__market_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    (void)fprintf(stderr, "[utax_market_data] ");
    (void)vfprintf(stderr, fmt, args);
    (void)fprintf(stderr, "\n");
    va_end(args);
}

static void utax__sleep_ms(unsigned ms) {
#if defined(_WIN32)
    Sleep(ms);
#else
    usleep((useconds_t)ms * 1000U);
#endif
}

typedef struct utax_fx_cache_entry {
    char currency[UTAX_CCY_MAX];
    char date_yyyy_mm_dd[11];
    double rate;
    int valid;
} utax_fx_cache_entry;

enum { UTAX_FX_CACHE_CAP = 32 };
static utax_fx_cache_entry g_utax_fx_cache[UTAX_FX_CACHE_CAP];
static unsigned g_utax_fx_cache_next = 0;

static int utax__fx_cache_get(const char *currency, const char *date_yyyy_mm_dd, double *out_rate) {
    unsigned i = 0;
    if (!currency || !date_yyyy_mm_dd || !out_rate) return 0;
    for (i = 0; i < UTAX_FX_CACHE_CAP; ++i) {
        if (!g_utax_fx_cache[i].valid) continue;
        if (strcmp(g_utax_fx_cache[i].currency, currency) == 0 &&
            strcmp(g_utax_fx_cache[i].date_yyyy_mm_dd, date_yyyy_mm_dd) == 0) {
            *out_rate = g_utax_fx_cache[i].rate;
            return 1;
        }
    }
    return 0;
}

static void utax__fx_cache_put(const char *currency, const char *date_yyyy_mm_dd, double rate) {
    utax_fx_cache_entry *e = NULL;
    if (!currency || !date_yyyy_mm_dd || rate <= 0.0) return;
    e = &g_utax_fx_cache[g_utax_fx_cache_next % UTAX_FX_CACHE_CAP];
    memset(e, 0, sizeof(*e));
    (void)snprintf(e->currency, sizeof(e->currency), "%s", currency);
    (void)snprintf(e->date_yyyy_mm_dd, sizeof(e->date_yyyy_mm_dd), "%s", date_yyyy_mm_dd);
    e->rate = rate;
    e->valid = 1;
    g_utax_fx_cache_next++;
}

static void utax__normalize_fx_currency(const char *in_ccy, char *out_ccy, size_t out_sz, double *out_rate_scale) {
    char u0 = 0, u1 = 0, u2 = 0;

    if (!out_ccy || out_sz == 0 || !out_rate_scale) return;
    out_ccy[0] = '\0';
    *out_rate_scale = 1.0;
    if (!in_ccy || !in_ccy[0]) return;

    u0 = (char)toupper((unsigned char)in_ccy[0]);
    u1 = (char)toupper((unsigned char)in_ccy[1]);
    u2 = (char)toupper((unsigned char)in_ccy[2]);

    /* London quotes in pence (GBp/GBX): request GBP from ECB and scale by 100. */
    if ((u0 == 'G' && u1 == 'B' && in_ccy[2] == 'p' && in_ccy[3] == '\0') ||
        (u0 == 'G' && u1 == 'B' && u2 == 'X' && in_ccy[3] == '\0')) {
        (void)snprintf(out_ccy, out_sz, "GBP");
        *out_rate_scale = 100.0;
        return;
    }

    /* Explicit GBP uppercase: keep as-is (major unit). */
    if (strcmp(in_ccy, "GBP") == 0) {
        (void)snprintf(out_ccy, out_sz, "GBP");
        *out_rate_scale = 1.0;
        return;
    }

    if (in_ccy[0] && in_ccy[1] && in_ccy[2] && in_ccy[3] == '\0') {
        char up[UTAX_CCY_MAX];
        up[0] = (char)toupper((unsigned char)in_ccy[0]);
        up[1] = (char)toupper((unsigned char)in_ccy[1]);
        up[2] = (char)toupper((unsigned char)in_ccy[2]);
        up[3] = '\0';
        (void)snprintf(out_ccy, out_sz, "%s", up);
        return;
    }

    (void)snprintf(out_ccy, out_sz, "%s", in_ccy);
}

static int utax__is_leap_year(int year) {
    return ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
}

static int utax__valid_ymd(int y, int m, int d) {
    static const int k_days[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    int max_day = 0;

    if (y < 1970 || y > 9999) return 0;
    if (m < 1 || m > 12) return 0;
    max_day = k_days[m - 1];
    if (m == 2 && utax__is_leap_year(y)) max_day = 29;
    return d >= 1 && d <= max_day;
}

static int utax__parse_yyyy_mm_dd(const char *s, int *out_y, int *out_m, int *out_d) {
    int y = 0, m = 0, d = 0;

    if (!s || !out_y || !out_m || !out_d) return 0;
    if (strlen(s) != 10) return 0;
    if (s[4] != '-' || s[7] != '-') return 0;
    if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]) ||
        !isdigit((unsigned char)s[2]) || !isdigit((unsigned char)s[3]) ||
        !isdigit((unsigned char)s[5]) || !isdigit((unsigned char)s[6]) ||
        !isdigit((unsigned char)s[8]) || !isdigit((unsigned char)s[9])) {
        return 0;
    }

    y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    m = (s[5] - '0') * 10 + (s[6] - '0');
    d = (s[8] - '0') * 10 + (s[9] - '0');
    if (!utax__valid_ymd(y, m, d)) return 0;

    *out_y = y;
    *out_m = m;
    *out_d = d;
    return 1;
}

static int64_t utax__days_from_civil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    {
        const int era = (y >= 0 ? y : y - 399) / 400;
        const unsigned yoe = (unsigned)(y - era * 400);
        const unsigned doy = (153U * (m + (m > 2 ? (unsigned)-3 : 9U)) + 2U) / 5U + d - 1U;
        const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
        return (int64_t)era * 146097 + (int64_t)doe - 719468;
    }
}

static void utax__civil_from_days(int64_t z, int *out_y, unsigned *out_m, unsigned *out_d) {
    z += 719468;
    {
        const int era = (z >= 0 ? (int)z : (int)(z - 146096)) / 146097;
        const unsigned doe = (unsigned)(z - (int64_t)era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int y = (int)yoe + era * 400;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        const unsigned d = doy - (153 * mp + 2) / 5 + 1;
        const unsigned m = mp + (mp < 10 ? 3 : (unsigned)-9);
        y += (m <= 2);

        if (out_y) *out_y = y;
        if (out_m) *out_m = m;
        if (out_d) *out_d = d;
    }
}

static int utax__format_yyyy_mm_dd(int y, unsigned m, unsigned d, char *out, size_t out_sz) {
    if (!out || out_sz < 11) return 0;
    if (snprintf(out, out_sz, "%04d-%02u-%02u", y, m, d) != 10) return 0;
    return 1;
}

static int utax__shift_date_days(const char *date_yyyy_mm_dd, int delta_days, char *out_date, size_t out_sz) {
    int y = 0, m = 0, d = 0;
    int64_t days = 0;
    int out_y = 0;
    unsigned out_m = 0, out_d = 0;

    if (!date_yyyy_mm_dd || !out_date || out_sz < 11) return 0;
    if (!utax__parse_yyyy_mm_dd(date_yyyy_mm_dd, &y, &m, &d)) return 0;

    days = utax__days_from_civil(y, (unsigned)m, (unsigned)d);
    days += (int64_t)delta_days;
    utax__civil_from_days(days, &out_y, &out_m, &out_d);
    return utax__format_yyyy_mm_dd(out_y, out_m, out_d, out_date, out_sz);
}

static int utax__date_to_unix_window(const char *date_yyyy_mm_dd, int64_t *out_period1, int64_t *out_period2) {
    int y = 0, m = 0, d = 0;
    int64_t days = 0;

    if (!out_period1 || !out_period2) return 0;
    if (!utax__parse_yyyy_mm_dd(date_yyyy_mm_dd, &y, &m, &d)) return 0;

    days = utax__days_from_civil(y, (unsigned)m, (unsigned)d);
    *out_period1 = days * 86400;
    *out_period2 = *out_period1 + 86400;
    return 1;
}

static int utax__valid_ticker(const char *ticker) {
    size_t i = 0;
    size_t n = 0;

    if (!ticker || !ticker[0]) return 0;
    n = strlen(ticker);
    if (n >= UTAX_TICKER_MAX) return 0;

    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)ticker[i];
        if (!(isalnum(c) || c == '.' || c == '-' || c == '_' || c == '=')) {
            return 0;
        }
    }
    return 1;
}

static const char *utax__skip_ws(const char *p) {
    while (p && *p && isspace((unsigned char)*p)) p++;
    return p;
}

static int utax__parse_first_array_double(const char *array_start, double *out_value, int *out_is_null) {
    const char *p = NULL;
    char *end = NULL;

    if (!array_start || !out_value || !out_is_null) return 0;
    p = strchr(array_start, '[');
    if (!p) return 0;

    p = utax__skip_ws(p + 1);
    if (!p || !*p) return 0;

    if (strncmp(p, "null", 4) == 0) {
        *out_is_null = 1;
        *out_value = 0.0;
        return 1;
    }

    *out_is_null = 0;
    *out_value = strtod(p, &end);
    return end != p;
}

static int utax__extract_array_value(const char *json, const char *key, double *out_value, int *out_is_null) {
    const char *p = NULL;
    if (!json || !key || !out_value || !out_is_null) return 0;
    p = strstr(json, key);
    if (!p) return 0;
    return utax__parse_first_array_double(p, out_value, out_is_null);
}

static int utax__extract_dividend_amount(const char *json, double *out_amount) {
    const char *p = NULL;
    char *end = NULL;

    if (!json || !out_amount) return 0;

    p = strstr(json, "\"dividends\"");
    if (!p) return 0;
    p = strstr(p, "\"amount\":");
    if (!p) return 0;

    p += 9;
    p = utax__skip_ws(p);
    if (!p || !*p) return 0;

    *out_amount = strtod(p, &end);
    return end != p;
}

static int utax__extract_adjclose_value(const char *json, double *out_value, int *out_is_null) {
    const char *outer = NULL;
    const char *inner = NULL;
    if (!json || !out_value || !out_is_null) return 0;

    outer = strstr(json, "\"adjclose\":[{");
    if (!outer) return 0;
    inner = strstr(outer + 1, "\"adjclose\":[");
    if (!inner) return 0;

    return utax__parse_first_array_double(inner, out_value, out_is_null);
}

static int utax__extract_meta_currency(const char *json, char *out_ccy, size_t out_ccy_sz) {
    const char *meta = NULL;
    const char *key = NULL;
    const char *val = NULL;
    char tmp[UTAX_CCY_MAX];
    size_t i = 0;

    if (!json || !out_ccy || out_ccy_sz == 0) return 0;
    out_ccy[0] = '\0';
    memset(tmp, 0, sizeof(tmp));

    meta = strstr(json, "\"meta\"");
    if (!meta) return 0;

    key = strstr(meta, "\"currency\"");
    if (!key) return 0;
    val = strchr(key, ':');
    if (!val) return 0;
    val = utax__skip_ws(val + 1);
    if (!val || *val != '"') return 0;
    val++;

    while (val[i] && val[i] != '"') {
        unsigned char c = (unsigned char)val[i];
        if (!isalpha(c)) return 0;
        if (i + 1 >= sizeof(tmp) || i + 1 >= out_ccy_sz) return 0;
        tmp[i] = (char)c;
        i++;
    }

    if (val[i] != '"' || i == 0) return 0;
    tmp[i] = '\0';
    (void)snprintf(out_ccy, out_ccy_sz, "%s", tmp);
    return 1;
}

static int utax__extract_ecb_csv_value(const char *csv, double *out_value) {
    const char *line = NULL;
    const char *p = NULL;
    const char *end_line = NULL;
    const char *val = NULL;
    char *end = NULL;
    size_t val_len = 0;
    char buf[64];

    if (!csv || !out_value) return 0;
    if (!*csv) return 0;

    /* last non-empty data line */
    p = csv + strlen(csv);
    while (p > csv && (p[-1] == '\n' || p[-1] == '\r' || isspace((unsigned char)p[-1]))) p--;
    if (p == csv) return 0;

    end_line = p;
    while (p > csv && p[-1] != '\n') p--;
    line = p;
    if (!line || line >= end_line) return 0;

    /* skip header line */
    if (strstr(line, "TIME_PERIOD") != NULL || strstr(line, "OBS_VALUE") != NULL) return 0;

    /* last CSV field */
    p = end_line;
    while (p > line && p[-1] != ',') p--;
    if (p <= line) return 0;
    val = p;
    while (val < end_line && isspace((unsigned char)*val)) val++;
    while (end_line > val && isspace((unsigned char)end_line[-1])) end_line--;
    if (val >= end_line) return 0;

    if (*val == '"' && end_line > val + 1 && end_line[-1] == '"') {
        val++;
        end_line--;
    }
    val_len = (size_t)(end_line - val);
    if (val_len == 0 || val_len >= sizeof(buf)) return 0;
    memcpy(buf, val, val_len);
    buf[val_len] = '\0';

    *out_value = strtod(buf, &end);
    if (end == buf) return 0;
    return *out_value > 0.0;
}

static utax_rc utax__lookup_yahoo_single_date(
    const char *ticker,
    const char *date_yyyy_mm_dd,
    int include_dividend_yield,
    utax_market_quote *out_quote
) {
    char url[UTAX_MARKET_URL_MAX];
    char json_buf[UTAX_MARKET_JSON_MAX];
    size_t json_len = 0;
    int64_t period1 = 0;
    int64_t period2 = 0;
    utax_rc rc = UTAX_OK;

    if (!ticker || !date_yyyy_mm_dd || !out_quote) return UTAX_ERR_INVALID_ARG;
    if (!utax__date_to_unix_window(date_yyyy_mm_dd, &period1, &period2)) return UTAX_ERR_INVALID_ARG;

    if (snprintf(
            url,
            sizeof(url),
            "https://query1.finance.yahoo.com/v8/finance/chart/%s?interval=1d&period1=%lld&period2=%lld&events=div",
            ticker,
            (long long)period1,
            (long long)period2
        ) >= (int)sizeof(url)) {
        return UTAX_ERR_TRUNCATED;
    }

    rc = utax__fetch_url_text(url, json_buf, sizeof(json_buf), &json_len);
    if (rc != UTAX_OK) return rc;
    if (json_len == 0) return UTAX_ERR_IO_READ;

    return utax_market_data_lookup_yahoo_date_from_json(
        ticker,
        date_yyyy_mm_dd,
        include_dividend_yield,
        json_buf,
        out_quote
    );
}

static utax_rc utax__fetch_url_text(const char *url, char *out_buf, size_t out_cap, size_t *out_len) {
    enum { UTAX_MAX_429_RETRIES = 3 };
    int attempt = 0;
    char cmd[UTAX_MARKET_CMD_MAX];

    if (!url || !out_buf || out_cap == 0 || !out_len) return UTAX_ERR_INVALID_ARG;

    for (attempt = 0; attempt <= UTAX_MAX_429_RETRIES; ++attempt) {
        FILE *pipe = NULL;
        size_t total = 0;
        size_t nread = 0;
        int status = 0;
        int is_429 = 0;
        int is_404 = 0;

        *out_len = 0;
        out_buf[0] = '\0';

        if (snprintf(
                cmd,
                sizeof(cmd),
                "curl -fsSL --retry 0 --max-time 15 -A \"ultimateTax/1.0\" \"%s\" 2>&1",
                url
            ) >= (int)sizeof(cmd)) {
            return UTAX_ERR_TRUNCATED;
        }

        pipe = UTAX_POPEN(cmd, "rb");
        if (!pipe) return UTAX_ERR_IO_OPEN;

        while ((nread = fread(out_buf + total, 1, out_cap - total - 1, pipe)) > 0) {
            total += nread;
            if (total >= out_cap - 1) {
                (void)UTAX_PCLOSE(pipe);
                out_buf[out_cap - 1] = '\0';
                return UTAX_ERR_NO_SPACE;
            }
        }

        out_buf[total] = '\0';
        status = UTAX_PCLOSE(pipe);
        if (status == 0 && total > 0) {
            *out_len = total;
            return UTAX_OK;
        }

        is_429 = (strstr(out_buf, "429") != NULL);
        is_404 = (strstr(out_buf, "404") != NULL);
        if (is_429) {
            utax__market_log("HTTP 429 rate limited while fetching URL: %s", url);
        } else if (is_404) {
            utax__market_log("HTTP 404 not found for URL: %s", url);
        } else {
            utax__market_log("fetch failed (status=%d, bytes=%zu) URL: %s", status, total, url);
        }
        if (total > 0) {
            utax__market_log("curl output: %s", out_buf);
        }

        if (is_429 && attempt < UTAX_MAX_429_RETRIES) {
            unsigned delay_ms = 500U << attempt; /* 500ms, 1000ms, 2000ms */
            utax__market_log(
                "retrying after 429 (attempt %d/%d, delay=%ums) URL: %s",
                attempt + 1,
                UTAX_MAX_429_RETRIES + 1,
                delay_ms,
                url
            );
            utax__sleep_ms(delay_ms);
            continue;
        }

        if (is_404) return UTAX_ERR_NOT_FOUND;
        return UTAX_ERR_IO_READ;
    }

    return UTAX_ERR_IO_READ;
}

static int utax__fetch_conversion_rate_eur(const char *currency, const char *date_yyyy_mm_dd, double *out_rate) {
    char fx_ccy[UTAX_CCY_MAX];
    char url[UTAX_MARKET_URL_MAX];
    char json_buf[UTAX_MARKET_JSON_MAX];
    char lookup_date[11];
    size_t json_len = 0;
    utax_rc rc = UTAX_OK;
    double v = 0.0;
    double rate_scale = 1.0;
    int back = 0;

    if (!currency || !date_yyyy_mm_dd || !out_rate) return 0;
    if (utax__fx_cache_get(currency, date_yyyy_mm_dd, out_rate)) return 1;
    utax__normalize_fx_currency(currency, fx_ccy, sizeof(fx_ccy), &rate_scale);
    if (fx_ccy[0] == '\0') return 0;
    if (strcmp(currency, fx_ccy) != 0 || rate_scale != 1.0) {
        utax__market_log("FX currency normalized: quote_ccy=%s api_ccy=%s rate_scale=%.2f", currency, fx_ccy, rate_scale);
    }

    if (strcmp(fx_ccy, "EUR") == 0) {
        *out_rate = 1.0;
        return 1;
    }

    for (back = 0; back <= 3; ++back) {
        if (!utax__shift_date_days(date_yyyy_mm_dd, -back, lookup_date, sizeof(lookup_date))) return 0;
        utax__sleep_ms(UTAX_MARKET_FX_CALL_THROTTLE_MS);

        if (snprintf(
                url,
                sizeof(url),
                "https://data-api.ecb.europa.eu/service/data/EXR/D.%s.EUR.SP00.A?startPeriod=%s&endPeriod=%s&detail=dataonly&format=csvdata",
                fx_ccy,
                lookup_date,
                lookup_date
            ) >= (int)sizeof(url)) {
            return 0;
        }

        rc = utax__fetch_url_text(url, json_buf, sizeof(json_buf), &json_len);
        if (rc != UTAX_OK || json_len == 0) {
            utax__market_log("FX lookup failed for %s->EUR on %s (fallback -%d day)", fx_ccy, lookup_date, back);
            continue;
        }
        if (!utax__extract_ecb_csv_value(json_buf, &v)) {
            utax__market_log("FX value missing for %s->EUR on %s (fallback -%d day)", fx_ccy, lookup_date, back);
            continue;
        }

        *out_rate = v * rate_scale;
        utax__fx_cache_put(currency, date_yyyy_mm_dd, *out_rate);
        return 1;
    }

    return 0;
}

utax_rc utax_market_data_lookup_yahoo_date_from_json(
    const char *ticker,
    const char *date_yyyy_mm_dd,
    int include_dividend_yield,
    const char *yahoo_chart_json,
    utax_market_quote *out_quote
) {
    double close_price = 0.0;
    double adjusted_close = 0.0;
    int close_is_null = 0;
    int adj_is_null = 0;
    double dividend_amount = 0.0;
    int has_dividend_amount = 0;

    if (!ticker || !date_yyyy_mm_dd || !yahoo_chart_json || !out_quote) return UTAX_ERR_INVALID_ARG;
    if (!utax__valid_ticker(ticker)) return UTAX_ERR_INVALID_ARG;
    if (!utax__parse_yyyy_mm_dd(date_yyyy_mm_dd, &(int){0}, &(int){0}, &(int){0})) return UTAX_ERR_INVALID_ARG;

    memset(out_quote, 0, sizeof(*out_quote));
    (void)snprintf(out_quote->ticker, sizeof(out_quote->ticker), "%s", ticker);
    (void)snprintf(out_quote->date_yyyy_mm_dd, sizeof(out_quote->date_yyyy_mm_dd), "%s", date_yyyy_mm_dd);
    out_quote->dividend_status = UTAX_MARKET_DIVIDEND_NOT_REQUESTED;
    out_quote->has_conversion_rate_eur = 0;

    if (!strstr(yahoo_chart_json, "\"chart\"")) return UTAX_ERR_NOT_FOUND;
    (void)utax__extract_meta_currency(yahoo_chart_json, out_quote->currency, sizeof(out_quote->currency));

    if (!utax__extract_array_value(yahoo_chart_json, "\"close\":", &close_price, &close_is_null)) {
        return UTAX_ERR_NOT_FOUND;
    }
    if (close_is_null) return UTAX_ERR_NOT_FOUND;
    out_quote->close_price = close_price;

    if (utax__extract_adjclose_value(yahoo_chart_json, &adjusted_close, &adj_is_null) && !adj_is_null) {
        out_quote->adjusted_close_price = adjusted_close;
        out_quote->has_adjusted_close = 1;
    }

    if (include_dividend_yield) {
        out_quote->dividend_status = UTAX_MARKET_DIVIDEND_UNAVAILABLE;
        has_dividend_amount = utax__extract_dividend_amount(yahoo_chart_json, &dividend_amount);
        if (has_dividend_amount && close_price > 0.0) {
            out_quote->dividend_amount = dividend_amount;
            out_quote->dividend_yield_pct = (dividend_amount / close_price) * 100.0;
            out_quote->dividend_status = UTAX_MARKET_DIVIDEND_AVAILABLE;
        }
    }

    return UTAX_OK;
}

utax_rc utax_market_data_lookup_yahoo_date(
    const char *ticker,
    const char *date_yyyy_mm_dd,
    int include_dividend_yield,
    utax_market_quote *out_quote
) {
    utax_market_quote q;
    char lookup_date[11];
    int back = 0;
    utax_rc rc = UTAX_OK;

    if (!ticker || !date_yyyy_mm_dd || !out_quote) return UTAX_ERR_INVALID_ARG;
    if (!utax__valid_ticker(ticker)) return UTAX_ERR_INVALID_ARG;
    if (!utax__parse_yyyy_mm_dd(date_yyyy_mm_dd, &(int){0}, &(int){0}, &(int){0})) return UTAX_ERR_INVALID_ARG;
    utax__sleep_ms(UTAX_MARKET_CALL_THROTTLE_MS);

    memset(&q, 0, sizeof(q));
    for (back = 0; back <= 3; ++back) {
        if (!utax__shift_date_days(date_yyyy_mm_dd, -back, lookup_date, sizeof(lookup_date))) return UTAX_ERR_INVALID_ARG;
        rc = utax__lookup_yahoo_single_date(ticker, lookup_date, include_dividend_yield, &q);
        if (rc != UTAX_OK) {
            utax__market_log(
                "quote lookup failed ticker=%s req_date=%s try_date=%s (fallback -%d day) rc=%d",
                ticker,
                date_yyyy_mm_dd,
                lookup_date,
                back,
                (int)rc
            );
        }
        if (rc == UTAX_OK) break;
        if (rc != UTAX_ERR_NOT_FOUND && rc != UTAX_ERR_PARSE) return rc;
    }
    if (rc != UTAX_OK) return UTAX_ERR_NOT_FOUND;

    if (q.currency[0]) {
        double conversion_rate_eur = 0.0;
        if (utax__fetch_conversion_rate_eur(q.currency, q.date_yyyy_mm_dd, &conversion_rate_eur)) {
            q.conversion_rate_eur = conversion_rate_eur;
            q.has_conversion_rate_eur = 1;
        }
    }

    *out_quote = q;
    return UTAX_OK;
}
