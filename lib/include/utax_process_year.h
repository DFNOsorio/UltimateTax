#pragma once
#include <stdint.h>
#include "utax_db.h"
#include "utax_schema.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct utax_process_year_realized_node {
    utax_fifo_realized_row row;
    struct utax_process_year_realized_node *next;
} utax_process_year_realized_node;

UTAX_API utax_rc process_year_trades(
    utax_db_t *db,
    uint16_t year,
    utax_process_year_realized_node **inout_head,
    size_t *inout_total_elems
);

UTAX_API void process_year_free_realized_list(
    utax_process_year_realized_node **inout_head,
    size_t *inout_total_elems
);

typedef struct utax_dividends_country_total_row {
    char country[UTAX_COUNTRY_MAX];
    double gross_amount_eur;
    double taxes_eur;
    double total_eur;
} utax_dividends_country_total_row;

typedef struct utax_dividends_country_total_node {
    utax_dividends_country_total_row row;
    struct utax_dividends_country_total_node *next;
} utax_dividends_country_total_node;

UTAX_API utax_rc process_year_dividends_country_totals(
    utax_db_t *db,
    uint16_t year,
    utax_dividends_country_total_node **inout_head,
    size_t *inout_total_elems
);

UTAX_API void process_year_free_dividends_country_total_list(
    utax_dividends_country_total_node **inout_head,
    size_t *inout_total_elems
);

#ifdef __cplusplus
}
#endif
