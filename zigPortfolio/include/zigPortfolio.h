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

// Must match src/trade.zig
enum {
    ZP_TRADE_DATETIME_CAP = 17, // "YYYY-MM-DD HH:MM" + '\0'
    ZP_TICKER_CAP = 16,
    ZP_BROKER_CAP = 8,
    ZP_TRADE_TYPE_CAP = 5,      // "BUY"/"SELL" + '\0'
    ZP_COUNTRY_CAP = 3,
    ZP_CURRENCY_CAP = 4,
};

typedef struct zp_trade {
    // Required (non-empty NUL-terminated strings)
    char trade_datetime[ZP_TRADE_DATETIME_CAP];
    char ticker[ZP_TICKER_CAP];

    // Required numerics
    double quantity;
    double price_per_share;

    // Optional/defaultable (empty string => use default)
    char broker[ZP_BROKER_CAP];         // "" => default "IKBR"
    char trade_type[ZP_TRADE_TYPE_CAP]; // "" => default "BUY"

    // Optional/defaultable numeric
    double commission;                  // NaN or <0 => default 0.0

    // Optional/defaultable
    char country[ZP_COUNTRY_CAP];       // "" => default "US"
    char currency[ZP_CURRENCY_CAP];     // "" => default "USD"

    // Optional/defaultable numeric
    double conversion_rate_eur;         // NaN or <=0 => default 1.0
} zp_trade;

zp_error_code zp_sqlite_open(const char *path, zp_db_handle *out_handle);

// Scalar-args insert (kept)
zp_error_code zp_sqlite_insert_trade(
    zp_db_handle handle,
    const char* trade_datetime,
    const char* ticker,
    double quantity,
    double price_per_share,
    const char* broker,          // pass NULL to default "IKBR"
    const char* type,            // pass NULL to default "BUY"
    double commission,           // pass NaN or <0 sentinel for “use default”
    const char* country,         // pass NULL to default "US"
    const char* currency,        // pass NULL to default "USD"
    double conversion_rate_eur   // pass NaN or <=0 sentinel to default 1.0
);

// Struct-based insert (new)
zp_error_code zp_sqlite_insert_trade_struct(
    zp_db_handle handle,
    const zp_trade* trade
);

// Read one trade by row id
zp_error_code zp_sqlite_read_trade_by_id(
    zp_db_handle handle,
    uint32_t id,
    zp_trade* out_trade
);

// Read all trades for a given year into out_trades[0..out_cap)
// out_count returns how many were written (may be < out_cap if truncated).
zp_error_code zp_sqlite_read_trades_by_year(
    zp_db_handle handle,
    uint32_t year,
    zp_trade* out_trades,
    size_t out_cap,
    size_t* out_count
);

zp_error_code zp_sqlite_read_trades_by_broker(
    zp_db_handle handle,
    const char* broker,      // required, non-NULL, non-empty
    zp_trade* out_trades,     // array
    size_t capacity,          // number of zp_trade slots in out_trades
    size_t* out_count         // returns how many rows were written
);

zp_error_code zp_sqlite_read_trades_by_year_and_broker(
    zp_db_handle handle,
    uint32_t year,
    const char* broker,
    zp_trade* out_trades,
    size_t capacity,
    size_t* out_count
);

zp_error_code zp_sqlite_close(zp_db_handle handle);

int    zp_version_major(void);
int    zp_version_minor(void);
int    zp_version_patch(void);
size_t zp_version_string(char *buf, size_t buf_len);

#ifdef __cplusplus
} // extern "C"
#endif
