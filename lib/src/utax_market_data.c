#include "utax_market_data.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
  #include <process.h>
  #define UTAX_POPEN _popen
  #define UTAX_PCLOSE _pclose
#else
  #include <sys/wait.h>
  #define UTAX_POPEN popen
  #define UTAX_PCLOSE pclose
#endif

enum {
    UTAX_MARKET_URL_MAX = 1024,
    UTAX_MARKET_CMD_MAX = 1200,
    UTAX_MARKET_JSON_MAX = 262144
};

static utax_rc utax__fetch_url_text(const char *url, char *out_buf, size_t out_cap, size_t *out_len);

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
    size_t i = 0;

    if (!json || !out_ccy || out_ccy_sz == 0) return 0;
    out_ccy[0] = '\0';

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
        if (!isupper(c)) return 0;
        if (i + 1 >= out_ccy_sz) return 0;
        out_ccy[i] = (char)c;
        i++;
    }

    if (val[i] != '"' || i == 0) return 0;
    out_ccy[i] = '\0';
    return 1;
}

static int utax__extract_ecb_value(const char *json, double *out_value) {
    const char *p = NULL;
    char *end = NULL;

    if (!json || !out_value) return 0;
    p = strstr(json, "\"value\":");
    if (!p) return 0;
    p += 8;
    p = utax__skip_ws(p);
    if (!p || !*p) return 0;
    if (*p == '"') p++;

    *out_value = strtod(p, &end);
    if (end == p) return 0;
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
    FILE *pipe = NULL;
    size_t total = 0;
    size_t nread = 0;
    int status = 0;
    char cmd[UTAX_MARKET_CMD_MAX];

    if (!url || !out_buf || out_cap == 0 || !out_len) return UTAX_ERR_INVALID_ARG;
    *out_len = 0;
    out_buf[0] = '\0';

    if (snprintf(cmd, sizeof(cmd), "curl -fsSL --max-time 15 \"%s\"", url) >= (int)sizeof(cmd)) {
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
    if (status != 0 || total == 0) return UTAX_ERR_IO_READ;

    *out_len = total;
    return UTAX_OK;
}

static int utax__fetch_conversion_rate_eur(const char *currency, const char *date_yyyy_mm_dd, double *out_rate) {
    char url[UTAX_MARKET_URL_MAX];
    char json_buf[UTAX_MARKET_JSON_MAX];
    char lookup_date[11];
    size_t json_len = 0;
    utax_rc rc = UTAX_OK;
    double v = 0.0;
    int back = 0;

    if (!currency || !date_yyyy_mm_dd || !out_rate) return 0;
    if (strcmp(currency, "EUR") == 0) {
        *out_rate = 1.0;
        return 1;
    }

    for (back = 0; back <= 3; ++back) {
        if (!utax__shift_date_days(date_yyyy_mm_dd, -back, lookup_date, sizeof(lookup_date))) return 0;

        if (snprintf(
                url,
                sizeof(url),
                "https://data-api.ecb.europa.eu/service/data/EXR/D.%s.EUR.SP00.A?startPeriod=%s&endPeriod=%s&format=jsondata",
                currency,
                lookup_date,
                lookup_date
            ) >= (int)sizeof(url)) {
            return 0;
        }

        rc = utax__fetch_url_text(url, json_buf, sizeof(json_buf), &json_len);
        if (rc != UTAX_OK || json_len == 0) continue;
        if (!utax__extract_ecb_value(json_buf, &v)) continue;

        *out_rate = v;
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

    if (!strstr(yahoo_chart_json, "\"chart\"")) return UTAX_ERR_PARSE;
    (void)utax__extract_meta_currency(yahoo_chart_json, out_quote->currency, sizeof(out_quote->currency));

    if (!utax__extract_array_value(yahoo_chart_json, "\"close\":", &close_price, &close_is_null)) {
        return UTAX_ERR_PARSE;
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

    memset(&q, 0, sizeof(q));
    for (back = 0; back <= 3; ++back) {
        if (!utax__shift_date_days(date_yyyy_mm_dd, -back, lookup_date, sizeof(lookup_date))) return UTAX_ERR_INVALID_ARG;
        rc = utax__lookup_yahoo_single_date(ticker, lookup_date, include_dividend_yield, &q);
        if (rc == UTAX_OK) break;
        if (rc != UTAX_ERR_NOT_FOUND) return rc;
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
