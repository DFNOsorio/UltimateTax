#include "utax_market_data.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int almost_equal(double a, double b) {
    double diff = fabs(a - b);
    return diff < 1e-9;
}

static void test_lookup_with_dividend(void) {
    const char *json =
        "{\"chart\":{\"result\":[{\"timestamp\":[1710374400],"
        "\"indicators\":{\"quote\":[{\"close\":[100.0]}],\"adjclose\":[{\"adjclose\":[99.5]}]},"
        "\"events\":{\"dividends\":{\"1710374400\":{\"amount\":1.25,\"date\":1710374400}}}}],\"error\":null}}";

    utax_market_quote q;
    utax_rc rc = utax_market_data_lookup_yahoo_date_from_json("AAPL", "2024-03-14", 1, json, &q);

    assert(rc == UTAX_OK);
    assert(strcmp(q.ticker, "AAPL") == 0);
    assert(strcmp(q.date_yyyy_mm_dd, "2024-03-14") == 0);
    assert(almost_equal(q.close_price, 100.0));
    assert(q.has_adjusted_close == 1);
    assert(almost_equal(q.adjusted_close_price, 99.5));
    assert(q.dividend_status == UTAX_MARKET_DIVIDEND_AVAILABLE);
    assert(almost_equal(q.dividend_amount, 1.25));
    assert(almost_equal(q.dividend_yield_pct, 1.25));
}

static void test_lookup_dividend_unavailable_non_error(void) {
    const char *json =
        "{\"chart\":{\"result\":[{\"timestamp\":[1710460800],"
        "\"indicators\":{\"quote\":[{\"close\":[250.0]}]}}],\"error\":null}}";

    utax_market_quote q;
    utax_rc rc = utax_market_data_lookup_yahoo_date_from_json("MSFT", "2024-03-15", 1, json, &q);

    assert(rc == UTAX_OK);
    assert(almost_equal(q.close_price, 250.0));
    assert(q.dividend_status == UTAX_MARKET_DIVIDEND_UNAVAILABLE);
    assert(almost_equal(q.dividend_amount, 0.0));
    assert(almost_equal(q.dividend_yield_pct, 0.0));
}

static void test_lookup_dividend_not_requested(void) {
    const char *json =
        "{\"chart\":{\"result\":[{\"timestamp\":[1710720000],"
        "\"indicators\":{\"quote\":[{\"close\":[321.45]}]}}],\"error\":null}}";

    utax_market_quote q;
    utax_rc rc = utax_market_data_lookup_yahoo_date_from_json("NVDA", "2024-03-18", 0, json, &q);

    assert(rc == UTAX_OK);
    assert(almost_equal(q.close_price, 321.45));
    assert(q.dividend_status == UTAX_MARKET_DIVIDEND_NOT_REQUESTED);
}

static void test_lookup_not_found_when_close_null(void) {
    const char *json =
        "{\"chart\":{\"result\":[{\"timestamp\":[1710806400],"
        "\"indicators\":{\"quote\":[{\"close\":[null]}]}}],\"error\":null}}";

    utax_market_quote q;
    utax_rc rc = utax_market_data_lookup_yahoo_date_from_json("TSLA", "2024-03-19", 1, json, &q);
    assert(rc == UTAX_ERR_NOT_FOUND);
}

static void test_lookup_invalid_args(void) {
    const char *json =
        "{\"chart\":{\"result\":[{\"timestamp\":[1710892800],"
        "\"indicators\":{\"quote\":[{\"close\":[180.0]}]}}],\"error\":null}}";

    assert(utax_market_data_lookup_yahoo_date_from_json(NULL, "2024-03-20", 1, json, NULL) == UTAX_ERR_INVALID_ARG);
    assert(utax_market_data_lookup_yahoo_date_from_json("AAPL", "2024-02-30", 1, json, NULL) == UTAX_ERR_INVALID_ARG);
    assert(utax_market_data_lookup_yahoo_date_from_json("AAP L", "2024-03-20", 1, json, NULL) == UTAX_ERR_INVALID_ARG);
    assert(utax_market_data_lookup_yahoo_date_from_json("AAPL", "2024-03-20", 1, NULL, NULL) == UTAX_ERR_INVALID_ARG);
}

int main(void) {
    test_lookup_with_dividend();
    test_lookup_dividend_unavailable_non_error();
    test_lookup_dividend_not_requested();
    test_lookup_not_found_when_close_null();
    test_lookup_invalid_args();

    printf("All ultimateTax market_data tests passed.\n");
    return 0;
}
