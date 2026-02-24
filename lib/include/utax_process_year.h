#pragma once
#include <stdint.h>
#include "utax_db.h"

#ifdef __cplusplus
extern "C" {
#endif


UTAX_API void process_year_trades(utax_db_t *db, uint16_t year);

#ifdef __cplusplus
}
#endif
