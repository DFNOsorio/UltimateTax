#pragma once

#include <stddef.h>

#include "utax_db.h"
#include "utax_process_year.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

UTAX_API void utax_utils_format_month_year(const char *iso_dt, char *out, size_t out_size);

UTAX_API utax_rc utax_utils_build_summary_report(
    const utax_fifo_realized_row *trade_rows,
    size_t trade_count,
    const utax_dividends_country_total_row *dividend_rows,
    size_t dividend_count,
    char **out_text
);

UTAX_API utax_rc utax_utils_write_text_file(const char *path, const char *text);

UTAX_API void utax_utils_free_text(char **inout_text);

#ifdef __cplusplus
}
#endif
