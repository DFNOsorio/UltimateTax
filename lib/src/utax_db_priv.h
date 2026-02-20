#ifndef ULTIMATETAX_UTAX_DB_PRIV_H
#define ULTIMATETAX_UTAX_DB_PRIV_H

#include "utax_db.h"
#include <sqlite3.h>



struct utax_db {
    sqlite3 *db;
    char last_errmsg[512];
};

utax_rc utax__set_err_sqlite(struct utax_db *h, int sqlite_rc);

#endif /* ULTIMATETAX_UTAX_DB_PRIV_H */
