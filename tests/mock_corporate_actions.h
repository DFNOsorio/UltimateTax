#pragma once

#include "utax_schema.h"
#include <stddef.h>

/* Mock corporate actions (ratio is generated in DB; keep 0.0 in mocks) */
const utax_corporate_actions_row UTAX_MOCK_CORP_ACTIONS[] = {
    /* 2020 */
    {
        .action_id = 0,
        .from_qty = 10000.0,
        .to_qty = 23348.0,
        .ratio = 0.0,
        .action_year = 0,

        .broker = "REVO",
        .action_date = "2020-04-02",
        .action_type = "MERGER",

        .from_ticker = "RTN",
        .to_ticker = "RTX"
    },
    {
        .action_id = 0,
        .from_qty = 1.0,
        .to_qty = 1.0,
        .ratio = 0.0,
        .action_year = 0,

        .broker = "REVO",
        .action_date = "2020-06-03",
        .action_type = "CONVERSION",

        .from_ticker = "VTIQ",
        .to_ticker = "NKLA"
    },
    {
        .action_id = 0,
        .from_qty = 100.0,
        .to_qty = 161.3,
        .ratio = 0.0,
        .action_year = 0,

        .broker = "IKBR",
        .action_date = "2020-11-16",
        .action_type = "SPINOFF",

        .from_ticker = "PFE",
        .to_ticker = "VTRS"
    },

    /* 2021 */
    {
        .action_id = 0,
        .from_qty = 8.0,
        .to_qty = 1.0,
        .ratio = 0.0,
        .action_year = 0,

        .broker = "REVO",
        .action_date = "2021-07-30",
        .action_type = "SPLIT",

        .from_ticker = "GE",
        .to_ticker = "" /* must be NULL in DB; API maps empty->NULL */
    },
    {
        .action_id = 0,
        .from_qty = 1.0,
        .to_qty = 1.0,
        .ratio = 0.0,
        .action_year = 0,

        .broker = "IKBR",
        .action_date = "2021-12-21",
        .action_type = "CONVERSION",

        .from_ticker = "PIC",
        .to_ticker = "XL"
    },
    {
        .action_id = 0,
        .from_qty = 1.0,
        .to_qty = 4.621214,
        .ratio = 0.0,
        .action_year = 0,

        .broker = "IKBR",
        .action_date = "2023-09-26",
        .action_type = "CASH",

        .from_ticker = "RNW.TO",
        .to_ticker = "" /* must be NULL in DB; API maps empty->NULL */
    }
};

inline size_t utax_mock_corp_actions_count(void) {
    return sizeof(UTAX_MOCK_CORP_ACTIONS) / sizeof(UTAX_MOCK_CORP_ACTIONS[0]);
}

/* CSV text using the header the parser expects */
const char *UTAX_CORP_ACTIONS_CSV_TEXT =
    "ACTION_DATE,BROKER,ACTION_TYPE,FROM_TICKER,FROM_QTY,TO_TICKER,TO_QTY\n"
    "2020-04-02,REVO,MERGER,RTN,10000,RTX,23348\n"
    "2020-06-03,REVO,CONVERSION,VTIQ,1,NKLA,1\n"
    "2021-07-30,REVO,SPLIT,GE,8,,1\n"
    "2023-09-26,IKBR,CASH,RNW.TO,1,,4.621214\n";
