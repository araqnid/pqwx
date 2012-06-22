/**
 * @file
 * Model classes for the object browser
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_model_h
#define __object_browser_model_h

/**
 * Base class for models of all database objects.
 */
class ObjectModel : public wxTreeItemData {
public:
  wxString name;
  wxString description;
  virtual wxString FormatName() const { return name; }
};

/**
 * A relation column.
 */
class ColumnModel : public ObjectModel {
public:
  RelationModel *relation;
  wxString type;
  bool nullable;
  bool hasDefault;
  int attnum;
};

/**
 * An index associated with a table.
 */
class IndexModel : public ObjectModel {
public:
  RelationModel *relation;
  bool primaryKey;
  bool unique;
  bool exclusion;
  bool clustered;
  std::vector<wxString> columns;
  wxString type;
};

/**
 * A trigger associated with a table.
 */
class TriggerModel : public ObjectModel {
public:
  RelationModel *relation;
};

/**
 * A schema member - function or relation.
 */
class SchemaMemberModel : public ObjectModel {
public:
  DatabaseModel *database;
  Oid oid;
  wxString schema;
  wxString extension;
  bool user;
  wxString FormatName() const { return schema + _T('.') + name; }
};

/**
 * A relation - table, view or sequence.
 */
class RelationModel : public SchemaMemberModel {
public:
  enum Type { TABLE, VIEW, SEQUENCE } type;
  int owningColumn; // for sequences that are attached to a column
};

/**
 * A function.
 */
class FunctionModel : public SchemaMemberModel {
public:
  wxString arguments;
  enum Type { SCALAR, RECORDSET, TRIGGER, AGGREGATE, WINDOW } type;
  wxString FormatName() const { return schema + _T('.') + name + _T('(') + arguments + _T(')'); }
};

/**
 * A database.
 *
 * This also contains a OID-to-tree-item lookup table and a CatalogueIndex.
 */
class DatabaseModel : public ObjectModel {
public:
  DatabaseModel() : catalogueIndex(NULL){}
  virtual ~DatabaseModel() {
    if (catalogueIndex != NULL)
      delete catalogueIndex;
  }
  Oid oid;
  bool isTemplate;
  bool allowConnections;
  bool havePrivsToConnect;
  bool loaded;
  ServerModel *server;
  const CatalogueIndex *catalogueIndex;
  bool IsUsable() const {
    return allowConnections && havePrivsToConnect;
  }
  bool IsSystem() const {
    return name == _T("postgres") || name == _T("template0") || name == _T("template1");
  }
  std::map<Oid, wxTreeItemId> symbolItemLookup;
  std::vector<RelationModel*> relations;
  std::vector<FunctionModel*> functions;

  /**
   * @return A string identifying this database, such as "[local] postgres"
   */
  wxString Identification() const;
  /**
   * Get a connection to this database.
   *
   * The connection returned may be still unconnected.
   */
  DatabaseConnection *GetDatabaseConnection();

  /**
   * The "divisions" of a database schema.
   */
  class Divisions {
  public:
    std::vector<SchemaMemberModel*> userDivision;
    std::vector<SchemaMemberModel*> systemDivision;
    std::map<wxString, std::vector<SchemaMemberModel*> > extensionDivisions;
  };

  /**
   * Divide the schema members up into user, system and extension members.
   */
  Divisions DivideSchemaMembers() const {
    std::vector<SchemaMemberModel*> members;
    for (std::vector<RelationModel*>::const_iterator iter = relations.begin(); iter != relations.end(); iter++) {
      members.push_back(*iter);
    }
    for (std::vector<FunctionModel*>::const_iterator iter = functions.begin(); iter != functions.end(); iter++) {
      members.push_back(*iter);
    }

    sort(members.begin(), members.end(), CollateSchemaMembers);
  
    Divisions result;

    for (std::vector<SchemaMemberModel*>::iterator iter = members.begin(); iter != members.end(); iter++) {
      SchemaMemberModel *member = *iter;
      if (!member->extension.IsEmpty())
	result.extensionDivisions[member->extension].push_back(member);
      else if (!member->user)
	result.systemDivision.push_back(member);
      else
	result.userDivision.push_back(member);
    }

    return result;
  }

private:
  static bool CollateSchemaMembers(SchemaMemberModel *r1, SchemaMemberModel *r2) {
    if (r1->schema < r2->schema) return true;
    if (r1->schema == r2->schema) {
      return r1->name < r2->name;
    }
    return false;
  }

};

/**
 * A server role.
 */
class RoleModel : public ObjectModel {
public:
  Oid oid;
  bool superuser;
  bool canLogin;
};

/**
 * A server.
 *
 * This class is also a factory for database connections. Currently at
 * most one connection for each database can be stored, and when a
 * connection to a new database is produced, any previous connection
 * is closed, so we maintain only one connection to a server at a
 * time.
 */
class ServerModel : public wxTreeItemData {
public:
  /**
   * Create server model.
   */
  ServerModel(const ServerConnection &conninfo) : conninfo(conninfo) {}
  /**
   * Create server model with an initial connection.
   */
  ServerModel(const ServerConnection &conninfo, DatabaseConnection *db) : conninfo(conninfo) {
    connections[db->DbName()] = db;
  }
  /**
   * Initialise some server parameters.
   *
   * @param serverVersionRaw Server version as string such as "9.1.1"
   * @param serverVersion_ Server version as an integer such as 90101
   * @param ssl SSL object (NULL if not SSL)
   */
  void ReadServerParameters(const char *serverVersionRaw, int serverVersion_, void *ssl) {
    serverVersion = serverVersion_;
    serverVersionString = wxString(serverVersionRaw, wxConvUTF8);
    usingSSL = (ssl != NULL);
  }
  /**
   * Close all connections associated with this server.
   */
  void Dispose();
  /**
   * Ask all connections associated with this server to disconnect.
   */
  void BeginDisconnectAll(std::vector<DatabaseConnection*> &disconnecting);
  /**
   * @return A string identifying this server, such as "dbhost:5433" or "steve@[local]"
   */
  const wxString& Identification() const { return conninfo.Identification(); }
  /**
   * @return The name of the "administrative" database
   */
  const wxString& GlobalDbName() const { return conninfo.globalDbName; }
  /**
   * @return The server version number as a string
   */
  const wxString& VersionString() const { return serverVersionString; }
  /**
   * @return True if server is using SSL (based on the initial connection)
   */
  bool IsUsingSSL() const { return usingSSL; }
  /**
   * Gets a connection to some database.
   *
   * The connection object returned may not be connected yet.
   */
  DatabaseConnection *GetDatabaseConnection(const wxString &dbname);
  /**
   * Gets a connection to the administrative database.
   *
   * The connection object returned may not be connected yet.
   */
  DatabaseConnection *GetServerAdminConnection() { return GetDatabaseConnection(GlobalDbName()); }
  /**
   * Find a named database model on this server.
   */
  DatabaseModel *FindDatabase(const wxString &dbname) const;
  /**
   * Server connection details.
   */
  const ServerConnection conninfo;
  /**
   * @return The databases on this server
   */
  const std::vector<DatabaseModel*>& GetDatabases() const { return databases; }
  /**
   * @return The roles on this server
   */
  const std::vector<RoleModel*>& GetRoles() const { return roles; }
private:
  std::vector<DatabaseModel*> databases;
  std::vector<RoleModel*> roles;
  int serverVersion;
  wxString serverVersionString;
  bool usingSSL;
  std::map<wxString, DatabaseConnection*> connections;
  friend class RefreshDatabaseListWork;
};

static inline bool emptySchema(std::vector<RelationModel*> schemaRelations) {
  return schemaRelations.size() == 1 && schemaRelations[0]->name.IsEmpty();
}

inline wxString DatabaseModel::Identification() const {
  return server->Identification() + _T(' ') + name;
}

inline DatabaseConnection *DatabaseModel::GetDatabaseConnection() {
  return server->GetDatabaseConnection(name);
}

#endif

// Local Variables:
// mode: c++
// End:
