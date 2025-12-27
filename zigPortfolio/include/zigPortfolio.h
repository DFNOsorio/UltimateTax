#pragma once

#include <cstdint>
#include <stdint.h>
#include <stddef.h>

typedef uintptr_t zp_db_handle;

typedef enum {
    ZP_ERROR_OK = 0,
    ZP_ERROR_INVALID_ARGUMENT = 1,
    ZP_ERROR_INTERNAL_ERROR = 2,
    ZP_ERROR_OPEN_FAIL,
    ZP_ERROR_CLOSE_FAIL,
    ZP_ERROR_PREPARATION_FAIL,
    ZP_ERROR_INSERTION_ERROR,
    ZP_ERROR_EXECUTION_FAIL
} zp_error_code;


zp_error_code zp_sqlite_open(const char *path, zp_db_handle *out_handle);
zp_error_code zp_sqlite_insert_trade(
    zp_db_handle handle,
    const char* trade_datetime,
    const char* ticker,
    double quantity,
    double price_per_share,
    const char* broker,          // pass NULL to default "IKBR"
    const char* type,            // pass NULL to default "BUY"
    double commission,           // pass NaN or negative sentinel if you want “use default”
    const char* country,         // pass NULL to default "US"
    const char* currency,        // pass NULL to default "USD"
    double conversion_rate_eur   // pass NaN or <=0 sentinel to default 1.0
);
zp_error_code zp_sqlite_close(zp_db_handle handle);

int  zp_version_major(void);
int  zp_version_minor(void);
int  zp_version_patch(void);
size_t zp_version_string(char *buf, size_t buf_len);
