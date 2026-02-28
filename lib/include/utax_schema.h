
#pragma once
/**
 * @file utax_schema.h
 * @brief Schema constants, row structures, and schema management APIs.
 */
#include "utax_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
   Fixed-size string field limits (including null terminator space)
   -------------------------------------------------------------------------- */
/** @brief Fixed-size limits for schema string fields (including null terminator). */
enum {
    UTAX_BROKER_MAX         = 16,  /* "IKBR", "REVOLUT", etc. */
    UTAX_DT_MAX             = 17,  /* "YYYY-MM-DD HH:MM" + '\0' */
    UTAX_TYPE_MAX           = 5,   /* "BUY"/"SELL" + '\0' */
    UTAX_TICKER_MAX         = 16,  /* e.g., "BRK.B", "RDSA.AS", etc. */
    UTAX_COUNTRY_MAX        = 16,  /* country/market code, supports values longer than 2 chars */
    UTAX_CCY_MAX            = 4,   /* "USD" + '\0' */
    UTAX_ACTION_TYPE_MAX    = 16   /* longest is "CONVERSION" (10) + '\0' */
};

/** @brief Row model for the `trades` table. */
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

/** @brief Row model for FIFO snapshot lots. */
typedef struct utax_fifo_snapshot_row {
    long long lot_id;
    long long acq_trade_id;

    double qty_remaining;
    double cost_per_share_eur;
    double acq_commission_eur;
    double last_updated_stock_price;
    double last_updated_stock_conversion_rate_eur;
    double current_lot_value_eur; /* generated in DB, but convenient to return */

    int tax_year;

    char broker[UTAX_BROKER_MAX];
    char ticker[UTAX_TICKER_MAX];
    char acq_datetime[UTAX_DT_MAX];
    char last_price_update_date[UTAX_DT_MAX];
    char last_updated_stock_currency[UTAX_CCY_MAX];
    char country[UTAX_COUNTRY_MAX];
} utax_fifo_snapshot_row;

/** @brief Row model for realized FIFO matches. */
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

/** @brief Link row between FIFO snapshot lots and corporate actions. */
typedef struct utax_fifo_snapshot_action_applied_row {
    long long lot_id;
    long long action_id;
} utax_fifo_snapshot_action_applied_row;

/** @brief Row model for the `dividends` table. */
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

/** @brief Row model for the `options_operations` table. */
typedef struct utax_options_row {
    long long option_id;

    int amount_x100;
    int bought_year;

    double per_contract;
    double tax;
    double conversion_rate_eur;

    char broker[UTAX_BROKER_MAX];
    char bought_dt[UTAX_DT_MAX];
    char expiration_dt[UTAX_DT_MAX];
    char ticker[UTAX_TICKER_MAX];
    char country[UTAX_COUNTRY_MAX];
    char currency[UTAX_CCY_MAX];
} utax_options_row;

/** @brief Row model for the `corporate_actions` table. */
typedef struct utax_corporate_actions_row {
    long long action_id;

    double from_qty;
    double to_qty;
    double ratio;          /* generated in DB, but convenient to return */

    int action_year;
    int _pad0;

    char broker[UTAX_BROKER_MAX];
    char action_date[UTAX_DT_MAX];                 /* "YYYY-MM-DD" */
    char action_type[UTAX_ACTION_TYPE_MAX];          /* MERGER/CONVERSION/SPINOFF/SPLIT */

    char from_ticker[UTAX_TICKER_MAX];
    char to_ticker[UTAX_TICKER_MAX];                 /* "" when NULL in DB (SPLIT) */
} utax_corporate_actions_row;

/**
 * @brief Applies schema SQL from file to the current database.
 * @param db Open database handle.
 * @param schema_sql_path UTF-8 path to a SQL script file.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
utax_rc utax_schema_apply_from_file(utax_db_t *db, const char *schema_sql_path);

/**
 * @brief Drops all schema objects managed by this project.
 * @param db Open database handle.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
utax_rc utax_schema_drop_all(utax_db_t *db);

/**
 * @brief Recreates schema by dropping objects and applying a SQL script.
 * @param db Open database handle.
 * @param schema_sql_path UTF-8 path to a SQL script file.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
utax_rc utax_schema_recreate_from_file(utax_db_t *db, const char *schema_sql_path);

/**
 * @brief Reads the SQLite `user_version` value.
 * @param db Open database handle.
 * @param out_version Output integer receiving the current schema version.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
utax_rc utax_schema_get_user_version(utax_db_t *db, int *out_version);
/**
 * @brief Sets the SQLite `user_version` value.
 * @param db Open database handle.
 * @param version New schema version value.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
utax_rc utax_schema_set_user_version(utax_db_t *db, int version);

#ifdef __cplusplus
}
#endif
