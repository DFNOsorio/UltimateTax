
#pragma once
#include "utax_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
   Fixed-size string field limits (including null terminator space)
   -------------------------------------------------------------------------- */
enum {
    UTAX_BROKER_MAX     = 16,  /* "IKBR", "REVOLUT", etc. */
    UTAX_DT_MAX         = 17,  /* "YYYY-MM-DD HH:MM" + '\0' */
    UTAX_TYPE_MAX       = 5,   /* "BUY"/"SELL" + '\0' */
    UTAX_TICKER_MAX     = 16,  /* e.g., "BRK.B", "RDSA.AS", etc. */
    UTAX_COUNTRY_MAX    = 4,   /* "US" + '\0' (room for padding) */
    UTAX_CCY_MAX        = 4    /* "USD" + '\0' */
};

typedef struct utax_trades_row {
    long long id;

    double quantity;
    double price_per_share;
    double commission;
    double conversion_rate_eur;

    int trade_year;

    char broker[UTAX_BROKER_MAX];
    char trade_datetime[UTAX_DT_MAX];
    char type[UTAX_TYPE_MAX];
    char ticker[UTAX_TICKER_MAX];
    char country[UTAX_COUNTRY_MAX];
    char currency[UTAX_CCY_MAX];
} utax_trades_row;

typedef struct utax_fifo_snapshot_row {
    long long lot_id;
    long long acq_trade_id;

    double qty_remaining;
    double cost_per_share_eur;
    double acq_commission_eur;

    int tax_year;

    char broker[UTAX_BROKER_MAX];
    char ticker[UTAX_TICKER_MAX];
    char acq_datetime[UTAX_DT_MAX];
    char country[UTAX_COUNTRY_MAX];
} utax_fifo_snapshot_row;

typedef struct utax_fifo_realized_row {
    long long realized_id;
    long long sell_trade_id;
    long long buy_trade_id;

    double qty_matched;
    double acquisition_value_eur;
    double sale_value_eur;
    double costs_eur;

    int tax_year;
    int match_seq;

    char broker[UTAX_BROKER_MAX];
    char ticker[UTAX_TICKER_MAX];
    char country[UTAX_COUNTRY_MAX];
    char sell_datetime[UTAX_DT_MAX];
    char buy_datetime[UTAX_DT_MAX];
} utax_fifo_realized_row;

typedef struct utax_dividends_row {
    long long dividend_id;

    double per_share;
    double total_amount;
    double tax;
    double conversion_rate_eur;

    /* derived year (stored generated column in DB) */
    int dividend_year;
    int _pad0;

    char broker[UTAX_BROKER_MAX];
    char dividend_dt[UTAX_DT_MAX];
    char ticker[UTAX_TICKER_MAX];
    char country[UTAX_COUNTRY_MAX];
    char currency[UTAX_CCY_MAX];
} utax_dividends_row;

utax_rc utax_schema_apply_from_file(utax_db_t *db, const char *schema_sql_path);

utax_rc utax_schema_drop_all(utax_db_t *db);

utax_rc utax_schema_recreate_from_file(utax_db_t *db, const char *schema_sql_path);

utax_rc utax_schema_get_user_version(utax_db_t *db, int *out_version);
utax_rc utax_schema_set_user_version(utax_db_t *db, int version);

#ifdef __cplusplus
}
#endif
