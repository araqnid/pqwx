#include "sql_dictionary.h"
#include "create_database_dialogue.h"

class CreateDatabaseDialogueSql : public SqlDictionary {
public:
    CreateDatabaseDialogueSql() {
AddSql(_T("List users"), "/* List users */ SELECT rolname FROM pg_roles ORDER BY 1");
AddSql(_T("List collations"), "/* List collations */ SELECT nspname, collname, pg_encoding_to_char(collencoding), collcollate, collctype FROM pg_collation JOIN pg_namespace ON pg_namespace.oid = pg_collation.collnamespace WHERE pg_collation.oid <> 100 /* omit 'default' */ AND pg_collation_is_visible(pg_collation.oid) ORDER BY CASE WHEN nspname = 'pg_catalog' THEN 1 ELSE 0 END, nspname, collname");

    }
};
const SqlDictionary& CreateDatabaseDialogue::GetSqlDictionary() {
  static CreateDatabaseDialogueSql obj;
  return obj;
}
