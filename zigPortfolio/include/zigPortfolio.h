#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t zp_db_handle;

typedef enum {
    ZP_ERROR_OK = 0,
    ZP_ERROR_INVALID_ARGUMENT = 1,
    ZP_ERROR_INTERNAL_ERROR = 2,
    ZP_ERROR_OPEN_FAIL = 3,
    ZP_ERROR_CLOSE_FAIL = 4,
    ZP_ERROR_PREPARATION_FAIL = 5,
    ZP_ERROR_INSERTION_ERROR = 6,
    ZP_ERROR_EXECUTION_FAIL = 7,
    ZP_ERROR_READ_ROW_FAIL = 8,
} zp_error_code;

/* Fixed buffer sizes used by ABI structs (must match Zig). */
enum {
    ZP_TRADE_DATETIME_CAP = 17, /* "YYYY-MM-DD HH:MM" + '\0' */
    ZP_TRADE_TICKER_CAP   = 16,
    ZP_TRADE_BROKER_CAP   = 8,
    ZP_TRADE_TYPE_CAP     = 8,
    ZP_TRADE_COUNTRY_CAP  = 8,
    ZP_TRADE_CURRENCY_CAP = 8,
};

typedef struct zp_trade {
    char   trade_datetime[ZP_TRADE_DATETIME_CAP]; /* required; non-empty */
    char   ticker[ZP_TRADE_TICKER_CAP];           /* required; non-empty */

    double quantity;                              /* required */
    double price_per_share;                       /* required */

    char   broker[ZP_TRADE_BROKER_CAP];           /* empty => default "IKBR" */
    char   trade_type[ZP_TRADE_TYPE_CAP];         /* empty => default "BUY" */

    double commission;                            /* NaN or <0 => default 0.0 */

    char   country[ZP_TRADE_COUNTRY_CAP];         /* empty => default "US" */
    char   currency[ZP_TRADE_CURRENCY_CAP];       /* empty => default "USD" */

    double conversion_rate_eur;                   /* NaN or <=0 => default 1.0 */
} zp_trade;

typedef struct zp_broker_name {
    char name[ZP_TRADE_BROKER_CAP];
} zp_broker_name;

/* fifo_snapshot row */
typedef struct zp_fifo_snapshot {
    uint32_t lot_id;                               /* output (AUTOINCREMENT) */

    char     broker[ZP_TRADE_BROKER_CAP];          /* required; non-empty */
    uint32_t tax_year;                             /* required */
    char     ticker[ZP_TRADE_TICKER_CAP];          /* required; non-empty */

    uint32_t acq_trade_id;                         /* required */
    char     acq_datetime[ZP_TRADE_DATETIME_CAP];  /* required; non-empty */

    double   qty_remaining;                        /* required; >= 0.0 */
    double   cost_per_share_eur;                   /* required; >= 0.0 */
    double   acq_commission_eur;                   /* NaN or <0 => default 0.0 */

    char     country[ZP_TRADE_COUNTRY_CAP];        /* required; non-empty */
} zp_fifo_snapshot;

/* fifo_realized row */
typedef struct zp_fifo_realized {
    uint32_t operation_id;                          /* output (AUTOINCREMENT) */

    char     broker[ZP_TRADE_BROKER_CAP];           /* required; non-empty */
    uint32_t tax_year;                              /* required */
    char     ticker[ZP_TRADE_TICKER_CAP];           /* required; non-empty */

    uint32_t sell_trade_id;                         /* required */
    uint32_t buy_trade_id;                          /* required */
    uint32_t match_seq;                             /* required (1..N per sell) */

    char     sell_datetime[ZP_TRADE_DATETIME_CAP];  /* required; non-empty */
    char     buy_datetime[ZP_TRADE_DATETIME_CAP];   /* required; non-empty */

    double   qty_matched;                           /* required; > 0.0 */
    double   proceeds_eur;                          /* required */
    double   cost_eur;                              /* required */
    double   gain_eur;                              /* required */
} zp_fifo_realized;

zp_error_code zp_sqlite_open(const char *path, zp_db_handle *out_handle);

/* Scalar insert */
zp_error_code zp_sqlite_insert_trade(
    zp_db_handle handle,
    const char* trade_datetime,
    const char* ticker,
    double quantity,
    double price_per_share,
    const char* broker,
    const char* trade_type,
    double commission,
    const char* country,
    const char* currency,
    double conversion_rate_eur
);

/* Struct-based insert */
zp_error_code zp_sqlite_insert_trade_struct(
    zp_db_handle handle,
    const zp_trade* trade
);

/* Read one by id */
zp_error_code zp_sqlite_read_trade_by_id(
    zp_db_handle handle,
    uint32_t id,
    zp_trade* out_trade
);

/*
 * Read list functions (Option B):
 * - If out == NULL and out_cap == 0, returns required count in out_count.
 * - Otherwise writes up to out_cap entries, sets out_count to number written, and truncates safely.
 */
zp_error_code zp_sqlite_read_trades_by_year(
    zp_db_handle handle,
    uint32_t year,
    zp_trade* out_trades,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_read_trades_by_broker(
    zp_db_handle handle,
    const char* broker,
    zp_trade* out_trades,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_read_trades_by_year_and_broker(
    zp_db_handle handle,
    uint32_t year,
    const char* broker,
    zp_trade* out_trades,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_read_all_trades(
    zp_db_handle handle,
    zp_trade* out_trades,
    size_t out_cap,
    size_t* out_count
);

/* Unique meta getters */
zp_error_code zp_sqlite_get_unique_brokers(
    zp_db_handle handle,
    zp_broker_name* out_brokers,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_get_unique_years(
    zp_db_handle handle,
    uint32_t* out_years,
    size_t out_cap,
    size_t* out_count
);

/* fifo_snapshot: insert + reads */
zp_error_code zp_sqlite_insert_fifo_snapshot(
    zp_db_handle handle,
    const zp_fifo_snapshot* row
);

zp_error_code zp_sqlite_read_fifo_snapshot_by_tax_year(
    zp_db_handle handle,
    uint32_t tax_year,
    zp_fifo_snapshot* out_rows,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_read_fifo_snapshot_all(
    zp_db_handle handle,
    zp_fifo_snapshot* out_rows,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_read_fifo_snapshot_by_ticker_per_year(
    zp_db_handle handle,
    uint32_t tax_year,
    const char* ticker,
    zp_fifo_snapshot* out_rows,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_read_fifo_snapshot_by_broker_per_year(
    zp_db_handle handle,
    uint32_t tax_year,
    const char* broker,
    zp_fifo_snapshot* out_rows,
    size_t out_cap,
    size_t* out_count
);

/* fifo_realized: insert + reads */
zp_error_code zp_sqlite_insert_fifo_realized(
    zp_db_handle handle,
    const zp_fifo_realized* row
);

zp_error_code zp_sqlite_read_fifo_realized_by_tax_year(
    zp_db_handle handle,
    uint32_t tax_year,
    zp_fifo_realized* out_rows,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_read_fifo_realized_all(
    zp_db_handle handle,
    zp_fifo_realized* out_rows,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_read_fifo_realized_by_ticker_per_year(
    zp_db_handle handle,
    uint32_t tax_year,
    const char* ticker,
    zp_fifo_realized* out_rows,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_read_fifo_realized_by_broker_per_year(
    zp_db_handle handle,
    uint32_t tax_year,
    const char* broker,
    zp_fifo_realized* out_rows,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_close(zp_db_handle handle);

typedef enum {
    ZP_TABLE_TRADES = 0,
    ZP_TABLE_FIFO_SNAPSHOT = 1,
    ZP_TABLE_FIFO_REALIZED = 2
} zp_table;

zp_error_code zp_sqlite_count_rows(
    zp_db_handle handle,
    zp_table table,
    const uint32_t* year,
    const char* broker,
    const char* ticker,
    size_t* out_count
);

/* Version */
int    zp_version_major(void);
int    zp_version_minor(void);
int    zp_version_patch(void);
size_t zp_version_string(char *buf, size_t buf_len);

#ifdef __cplusplus
} /* extern "C" */
#endif
