#pragma once

#include <stdint.h>
#include <stddef.h>

typedef uintptr_t zp_db_handle;

typedef enum {
    ZP_ERROR_OK = 0,
    ZP_ERROR_INVALID_ARGUMENT = 1,
    ZP_ERROR_INTERNAL_ERROR = 2,
} zp_error_code;


zp_error_code zp_sqlite_open(const char *path, zp_db_handle *out_handle);
zp_error_code zp_sqlite_close(zp_db_handle handle);

int  zp_version_major(void);
int  zp_version_minor(void);
int  zp_version_patch(void);
size_t zp_version_string(char *buf, size_t buf_len);
