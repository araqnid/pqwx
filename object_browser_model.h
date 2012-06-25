/**
 * @file
 * Model classes for the object browser
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_model_h
#define __object_browser_model_h

#include <openssl/ssl.h>

/**
 * A "soft" reference into the object model.
 */
class ObjectModelReference {
public:
  ObjectModelReference(const wxString& serverId) : serverId(serverId), database(InvalidOid), regclass(InvalidOid), oid(InvalidOid), subid(0) {}
  ObjectModelReference(const wxString& serverId, Oid database) : serverId(serverId), database(database), regclass(PG_DATABASE), oid(database), subid(0) {}
  ObjectModelReference(const wxString& serverId, Oid regclass, Oid oid) : serverId(serverId), database(InvalidOid), regclass(regclass), oid(oid), subid(0) {}
  ObjectModelReference(const ObjectModelReference& database, Oid regclass, Oid oid, int subid = 0) : serverId(database.serverId), database(database.oid), regclass(regclass), oid(oid), subid(subid) {}

  wxString Identify() const
  {
    wxString buf;
    buf << _T("Server#") << serverId;
    if (database != InvalidOid) {
      buf << _T("/") << database;
      if (regclass != PG_DATABASE) {
        buf << _T("/") << regclass << _T(":") << oid;
        if (subid)
          buf << _T(".") << subid;
      }
    }
    return buf;
  }

  wxString GetServerId() const { return serverId; }
  Oid GetDatabase() const { return database; }
  Oid GetObjectClass() const { return regclass; }
  Oid GetOid() const { return oid; }
  int GetObjectSubid() const { return subid; }

  ObjectModelReference DatabaseRef() const { return ObjectModelReference(serverId, database); }

  bool operator<(const ObjectModelReference& other) const
  {
    if (serverId < other.serverId) return true;
    if (regclass < other.regclass) return true;
    if (database < other.database) return true;
    if (regclass == PG_DATABASE) return false;
    if (oid < other.oid) return true;
    return subid < other.subid;
  }

  static const Oid PG_ATTRIBUTE = 1249;
  static const Oid PG_CLASS = 1259;
  static const Oid PG_CONSTRAINT = 2606;
  static const Oid PG_DATABASE = 1262;
  static const Oid PG_INDEX = 2610;
  static const Oid PG_PROC = 1255;
  static const Oid PG_ROLE = 1260;
  static const Oid PG_TABLESPACE = 1213;
  static const Oid PG_TRIGGER = 2620;
  static const Oid PG_TS_CONFIG = 3602;
  static const Oid PG_TS_DICT = 3600;
  static const Oid PG_TS_PARSER = 3601;
  static const Oid PG_TS_TEMPLATE = 3764;
private:
  wxString serverId;
  Oid database;
  Oid regclass;
  Oid oid;
  int subid;
};

class RelationModel;
class DatabaseModel;
class ServerModel;
class ObjectBrowser;

/**
 * Base class for models of all database objects.
 */
class ObjectModel {
public:
  wxString name;
  wxString description;
  virtual wxString FormatName() const { return name; }
  static bool CollateByName(const ObjectModel& o1, const ObjectModel& o2) {
    return o1.name < o2.name;
  }
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
  class Column {
  public:
    Column(int column_, const wxString& expression_) : column(column_), expression(expression_) {}
    int column;
    wxString expression;
  };
  RelationModel *relation;
  Oid oid;
  bool primaryKey;
  bool unique;
  bool exclusion;
  bool clustered;
  std::vector<Column> columns;
  wxString type;
};

/**
 * A trigger associated with a table.
 */
class TriggerModel : public ObjectModel {
public:
  RelationModel *relation;
  Oid oid;
};

/**
 * A check constraint associated with a table.
 */
class CheckConstraintModel : public ObjectModel {
public:
  RelationModel *relation;
  wxString expression;
  Oid oid;
};


/**
 * A schema member - function or relation.
 */
class SchemaMemberModel : public ObjectModel {
public:
  Oid oid;
  wxString schema;
  wxString extension;
  wxString FormatName() const { return schema + _T('.') + name; }
  bool IsUser() const { return !IsSystemSchema(schema); }
  static inline bool IsSystemSchema(wxString schema) {
    return schema.StartsWith(_T("pg_")) || schema == _T("information_schema");
  }
  static bool CollateByQualifiedName(SchemaMemberModel *r1, SchemaMemberModel *r2) {
    if (r1->schema < r2->schema) return true;
    if (r1->schema == r2->schema) {
      return r1->name < r2->name;
    }
    return false;
  }
};

/**
 * A relation - table, view or sequence.
 */
class RelationModel : public SchemaMemberModel {
public:
  enum Type { TABLE, VIEW, SEQUENCE } type;
  int owningColumn; // for sequences that are attached to a column
  bool unlogged;
  std::vector<ColumnModel*> columns;
  std::vector<IndexModel*> indices;
  std::vector<TriggerModel*> triggers;
  std::vector<RelationModel*> sequences;
  std::vector<CheckConstraintModel*> checkConstraints;
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
 * A full-text search configuration.
 */
class TextSearchConfigurationModel : public SchemaMemberModel {
public:
};

/**
 * A full-text search parser.
 */
class TextSearchParserModel : public SchemaMemberModel {
public:
};

/**
 * A full-text search configuration.
 */
class TextSearchDictionaryModel : public SchemaMemberModel {
public:
};

/**
 * A full-text search configuration.
 */
class TextSearchTemplateModel : public SchemaMemberModel {
public:
};

/**
 * A tablespace.
 */
class TablespaceModel : public ObjectModel {
public:
  Oid oid;
  wxString location;
  bool IsSystem() const { return name.StartsWith(_T("pg_")); }
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
  operator ObjectModelReference () const;
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
  ObjectModel *FindObject(const ObjectModelReference& ref) const;
  std::vector<RelationModel*> relations;
  std::vector<FunctionModel*> functions;
  std::vector<TextSearchDictionaryModel*> textSearchDictionaries;
  std::vector<TextSearchParserModel*> textSearchParsers;
  std::vector<TextSearchTemplateModel*> textSearchTemplates;
  std::vector<TextSearchConfigurationModel*> textSearchConfigurations;

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
    std::vector<const SchemaMemberModel*> userDivision;
    std::vector<const SchemaMemberModel*> systemDivision;
    std::map<wxString, std::vector<const SchemaMemberModel*> > extensionDivisions;
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
    for (std::vector<TextSearchDictionaryModel*>::const_iterator iter = textSearchDictionaries.begin(); iter != textSearchDictionaries.end(); iter++) {
      members.push_back(*iter);
    }
    for (std::vector<TextSearchParserModel*>::const_iterator iter = textSearchParsers.begin(); iter != textSearchParsers.end(); iter++) {
      members.push_back(*iter);
    }
    for (std::vector<TextSearchTemplateModel*>::const_iterator iter = textSearchTemplates.begin(); iter != textSearchTemplates.end(); iter++) {
      members.push_back(*iter);
    }
    for (std::vector<TextSearchConfigurationModel*>::const_iterator iter = textSearchConfigurations.begin(); iter != textSearchConfigurations.end(); iter++) {
      members.push_back(*iter);
    }

    sort(members.begin(), members.end(), SchemaMemberModel::CollateByQualifiedName);
  
    Divisions result;

    for (std::vector<SchemaMemberModel*>::iterator iter = members.begin(); iter != members.end(); iter++) {
      SchemaMemberModel *member = *iter;
      if (!member->extension.IsEmpty())
        result.extensionDivisions[member->extension].push_back(member);
      else if (!member->IsUser())
        result.systemDivision.push_back(member);
      else
        result.userDivision.push_back(member);
    }

    return result;
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
class ServerModel : public ObjectModel {
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
   * @param serverVersionString_ Server version as string such as "9.1.1"
   * @param serverVersion_ Server version as an integer such as 90101
   * @param ssl SSL object (NULL if not SSL)
   */
  void UpdateServerParameters(const wxString& serverVersionString_, int serverVersion_, SSL *ssl) {
    serverVersionString = serverVersionString_;
    serverVersion = serverVersion_;
    if (ssl != NULL) {
      sslCipher = wxString(SSL_get_cipher(ssl), wxConvUTF8);
    }
  }
  void UpdateDatabases(const std::vector<DatabaseModel>& incoming)
  {
    databases.clear();
    databases.reserve(incoming.size());
    for (std::vector<DatabaseModel>::const_iterator iter = incoming.begin(); iter != incoming.end(); iter++) {
      databases.push_back(*iter);
      databases.back().server = this;
    }
  }
  void UpdateDatabase(const DatabaseModel& incoming)
  {
    for (std::vector<DatabaseModel>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
      if ((*iter).oid == incoming.oid) {
        wxString dbname = (*iter).name;
        *iter = incoming;
        (*iter).server = this;
        (*iter).name = dbname;
        (*iter).loaded = true;
        return;
      }
    }
    wxASSERT(false);
  }
  void UpdateRoles(const std::vector<RoleModel>& incoming)
  {
    roles.clear();
    roles.reserve(incoming.size());
    for (std::vector<RoleModel>::const_iterator iter = incoming.begin(); iter != incoming.end(); iter++) {
      roles.push_back(*iter);
    }
  }
  void UpdateTablespaces(const std::vector<TablespaceModel>& incoming)
  {
    tablespaces.clear();
    tablespaces.reserve(incoming.size());
    for (std::vector<TablespaceModel>::const_iterator iter = incoming.begin(); iter != incoming.end(); iter++) {
      tablespaces.push_back(*iter);
    }
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
  bool IsUsingSSL() const { return !sslCipher.empty(); }
  /**
   * @return The name of the SSL cipher in use, if applicable
   */
  const wxString& GetSSLCipher() const { return sslCipher; }
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
  DatabaseModel *FindDatabase(const wxString &dbname);
  /**
   * Find a database model on this server.
   */
  DatabaseModel *FindDatabase(const ObjectModelReference &ref)
  {
    wxASSERT(ref.GetObjectClass() == ObjectModelReference::PG_DATABASE);
    return FindDatabaseByOid(ref.GetOid());
  }
  /**
   * Find an arbitrary object on this server.
   */
  ObjectModel *FindObject(const ObjectModelReference &ref);
  /**
   * Server connection details.
   */
  const ServerConnection conninfo;
  /**
   * @return The databases on this server
   */
  const std::vector<DatabaseModel>& GetDatabases() const { return databases; }
  /**
   * @return The roles on this server
   */
  const std::vector<RoleModel>& GetRoles() const { return roles; }
  /**
   * @return The tablespaces on this server
   */
  const std::vector<TablespaceModel>& GetTablespaces() const { return tablespaces; }
private:
  std::vector<DatabaseModel> databases;
  std::vector<TablespaceModel> tablespaces;
  std::vector<RoleModel> roles;
  int serverVersion;
  wxString serverVersionString;
  wxString sslCipher;
  std::map<wxString, DatabaseConnection*> connections;
  DatabaseModel *FindDatabaseByOid(Oid oid);
  friend class RefreshDatabaseListWork;
};

/**
 * The "top level" of the object browser model.
 */
class ObjectBrowserModel : public wxEvtHandler {
public:
  /**
   * Find an existing server matching some connection parameters.
   */
  ServerModel *FindServer(const ServerConnection &server) const { return FindServerById(server.Identification()); }
  /**
   * Find an existing server by reference.
   */
  ServerModel *FindServer(const ObjectModelReference &ref) const { return FindServerById(ref.GetServerId()); }

  /**
   * Find an existing database matching some connection parameters and database name.
   */
  DatabaseModel *FindDatabase(const ServerConnection &server, const wxString &dbname) const;
  /**
   * Find an existing database by reference.
   */
  DatabaseModel *FindDatabase(const ObjectModelReference &ref) const;
  /**
   * Register a new server connection with the object browser.
   */
  ServerModel* AddServerConnection(const ServerConnection &conninfo, DatabaseConnection *db);
  /**
   * Close all server connections and delete all model data.
   */
  void Dispose();
  /**
   * Remove a server.
   */
  void RemoveServer(ServerModel *server);
  void SetupDatabaseConnection(DatabaseConnection *db);

  /**
   * Find any object by reference.
   */
  ObjectModel *FindObject(const ObjectModelReference &ref) const;

  /**
   * Find a particular relation.
   */
  RelationModel *FindRelation(const ObjectModelReference &ref) const
  {
    wxASSERT(ref.GetObjectClass() == ObjectModelReference::PG_CLASS);
    ObjectModel *obj = FindObject(ref);
    if (obj == NULL) return NULL;
    return static_cast<RelationModel*>(obj);
  }

  /**
   * Find a particular function.
   */
  FunctionModel *FindFunction(const ObjectModelReference &ref) const
  {
    wxASSERT(ref.GetObjectClass() == ObjectModelReference::PG_PROC);
    ObjectModel *obj = FindObject(ref);
    if (obj == NULL) return NULL;
    return static_cast<FunctionModel*>(obj);
  }

private:
  ServerModel *FindServerById(const wxString&) const;
  std::list<ServerModel*> servers;
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

inline DatabaseModel::operator ObjectModelReference() const
{
  return ObjectModelReference(server->Identification(), oid);
}

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
