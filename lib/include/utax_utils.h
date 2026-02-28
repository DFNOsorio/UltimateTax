#pragma once
/**
 * @file utax_utils.h
 * @brief Utility helpers for report formatting and text file output.
 */

#include <stddef.h>

#include "utax_db.h"
#include "utax_process_year.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Formats an ISO datetime into a month-year label.
 * @param iso_dt Input ISO-like datetime string.
 * @param out Output character buffer.
 * @param out_size Size of @p out in bytes.
 */
UTAX_API void utax_utils_format_month_year(const char *iso_dt, char *out, size_t out_size);

/**
 * @brief Builds a textual summary report from trade and dividend aggregates.
 * @param trade_rows Trade aggregate rows.
 * @param trade_count Number of elements in @p trade_rows.
 * @param dividend_rows Dividend aggregate rows.
 * @param dividend_count Number of elements in @p dividend_rows.
 * @param out_text Output UTF-8 text buffer allocated by the function.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_utils_build_summary_report(
    const utax_fifo_realized_row *trade_rows,
    size_t trade_count,
    const utax_dividends_country_total_row *dividend_rows,
    size_t dividend_count,
    char **out_text
);

/**
 * @brief Writes a UTF-8 text buffer to a file path.
 * @param path Output file path.
 * @param text Null-terminated text buffer to write.
 * @return @ref UTAX_OK on success, otherwise an error code.
 */
UTAX_API utax_rc utax_utils_write_text_file(const char *path, const char *text);

/**
 * @brief Frees text allocated by utility report builders.
 * @param inout_text In/out pointer to allocated text pointer; set to NULL on return.
 */
UTAX_API void utax_utils_free_text(char **inout_text);

#ifdef __cplusplus
}
#endif
