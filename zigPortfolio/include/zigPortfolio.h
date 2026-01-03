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

static inline const char* zp_error_to_string(zp_error_code ec) {
    switch (ec) {
        case ZP_ERROR_OK: return "ZP_ERROR_OK";
        case ZP_ERROR_INVALID_ARGUMENT: return "ZP_ERROR_INVALID_ARGUMENT";
        case ZP_ERROR_INTERNAL_ERROR: return "ZP_ERROR_INTERNAL_ERROR";
        case ZP_ERROR_OPEN_FAIL: return "ZP_ERROR_OPEN_FAIL";
        case ZP_ERROR_CLOSE_FAIL: return "ZP_ERROR_CLOSE_FAIL";
        case ZP_ERROR_PREPARATION_FAIL: return "ZP_ERROR_PREPARATION_FAIL";
        case ZP_ERROR_INSERTION_ERROR: return "ZP_ERROR_INSERTION_ERROR";
        case ZP_ERROR_EXECUTION_FAIL: return "ZP_ERROR_EXECUTION_FAIL";
        case ZP_ERROR_READ_ROW_FAIL: return "ZP_ERROR_READ_ROW_FAIL";
        default: return "ZP_ERROR_<unknown>";
    }
}

/* Fixed buffer sizes used by ABI structs (MUST match schemaStructs.zig). */
enum {
    ZP_BROKER_LEN   = 64,
    ZP_TICKER_LEN   = 32,
    ZP_DATETIME_LEN = 32, /* "YYYY-MM-DD HH:MM" + '\0' */
    ZP_TYPE_LEN     = 8,
    ZP_COUNTRY_LEN  = 16,
    ZP_CURRENCY_LEN = 8,
};

typedef struct zp_trade {
    uint32_t id;                          /* output (AUTOINCREMENT / row id) */

    char broker[ZP_BROKER_LEN];
    char trade_datetime[ZP_DATETIME_LEN];
    char trade_type[ZP_TYPE_LEN];
    char ticker[ZP_TICKER_LEN];

    double quantity;
    double price_per_share;
    double commission;

    char country[ZP_COUNTRY_LEN];
    char currency[ZP_CURRENCY_LEN];
    double conversion_rate_eur;
} zp_trade;


typedef struct zp_broker_name {
    char name[ZP_BROKER_LEN];
} zp_broker_name;

/* fifo_snapshot row */
typedef struct zp_fifo_snapshot {
    uint32_t lot_id;                               /* output (AUTOINCREMENT) */

    char     broker[ZP_BROKER_LEN];          /* required; non-empty */
    uint32_t tax_year;                             /* required */
    char     ticker[ZP_TICKER_LEN];          /* required; non-empty */

    uint32_t acq_trade_id;                         /* required */
    char     acq_datetime[ZP_DATETIME_LEN];  /* required; non-empty */

    double   qty_remaining;                        /* required; >= 0.0 */
    double   cost_per_share_eur;                   /* required; >= 0.0 */
    double   acq_commission_eur;                   /* NaN or <0 => default 0.0 */

    char     country[ZP_COUNTRY_LEN];        /* required; non-empty */
} zp_fifo_snapshot;

/* fifo_realized row */
typedef struct zp_fifo_realized {
    uint32_t operation_id;                          /* output (AUTOINCREMENT) */

    char     broker[ZP_BROKER_LEN];           /* required; non-empty */
    uint32_t tax_year;                              /* required */
    char     ticker[ZP_TICKER_LEN];           /* required; non-empty */

    uint32_t sell_trade_id;                         /* required */
    uint32_t buy_trade_id;                          /* required */
    uint32_t match_seq;                             /* required (1..N per sell) */

    char     sell_datetime[ZP_DATETIME_LEN];  /* required; non-empty */
    char     buy_datetime[ZP_DATETIME_LEN];   /* required; non-empty */

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

zp_error_code zp_sqlite_process_year_load_only(
    zp_db_handle handle,
    uint32_t year
);

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
