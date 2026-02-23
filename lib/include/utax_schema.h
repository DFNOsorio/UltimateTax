
#pragma once
#include "utax_db.h"


utax_rc utax_schema_apply_from_file(utax_db_t *db, const char *schema_sql_path);

utax_rc utax_schema_drop_all(utax_db_t *db);

utax_rc utax_schema_recreate_from_file(utax_db_t *db, const char *schema_sql_path);

utax_rc utax_schema_get_user_version(utax_db_t *db, int *out_version);
utax_rc utax_schema_set_user_version(utax_db_t *db, int version);


