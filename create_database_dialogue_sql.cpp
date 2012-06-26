#include "sql_dictionary.h"
#include "create_database_dialogue.h"

class CreateDatabaseDialogueSql : public SqlDictionary {
public:
    CreateDatabaseDialogueSql() {
AddSql(_T("List users"), "/* List users */ SELECT rolname FROM pg_roles ORDER BY 1");

    }
};
const SqlDictionary& CreateDatabaseDialogue::GetSqlDictionary() {
  static CreateDatabaseDialogueSql obj;
  return obj;
}
