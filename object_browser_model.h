/**
 * @file
 * Model classes for the object browser
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_model_h
#define __object_browser_model_h

#include <openssl/ssl.h>

BEGIN_DECLARE_EVENT_TYPES()
  DECLARE_EVENT_TYPE(PQWX_ObjectBrowserWorkFinished, -1)
  DECLARE_EVENT_TYPE(PQWX_ObjectBrowserWorkCrashed, -1)
  DECLARE_EVENT_TYPE(PQWX_RescheduleObjectBrowserWork, -1)
END_DECLARE_EVENT_TYPES()

/**
 * A "soft" reference into the object model.
 */
class ObjectModelReference {
public:
  ObjectModelReference(const wxString& serverId) : serverId(serverId), database(InvalidOid), regclass(InvalidOid), oid(InvalidOid), subid(0) {}
  ObjectModelReference(const wxString& serverId, Oid database) : serverId(serverId), database(database), regclass(PG_DATABASE), oid(database), subid(0) {}
  ObjectModelReference(const wxString& serverId, Oid regclass, Oid oid) : serverId(serverId), database(InvalidOid), regclass(regclass), oid(oid), subid(0) {}
  ObjectModelReference(const ObjectModelReference& database, Oid regclass, Oid oid, int subid = 0) : serverId(database.serverId), database(database.oid), regclass(regclass), oid(oid), subid(subid) {}

  wxString Identify() const;

  wxString GetServerId() const { return serverId; }
  Oid GetDatabase() const { return database; }
  Oid GetObjectClass() const { return regclass; }
  Oid GetOid() const { return oid; }
  int GetObjectSubid() const { return subid; }

  ObjectModelReference ServerRef() const { return ObjectModelReference(serverId); }
  ObjectModelReference DatabaseRef() const { return ObjectModelReference(serverId, database); }

  bool operator<(const ObjectModelReference& other) const
  {
    if (serverId < other.serverId) return true;
    else if (serverId > other.serverId) return false;
    if (regclass < other.regclass) return true;
    else if (regclass > other.regclass) return false;
    if (database < other.database) return true;
    else if (database > other.database) return false;
    if (oid < other.oid) return true;
    else if (oid > other.oid) return false;
    return subid < other.subid;
  }

  bool operator==(const ObjectModelReference& other) const
  {
    if (&other == this) return true;
    if (serverId != other.serverId) return false;
    if (database != other.database) return false;
    if (regclass != other.regclass) return false;
    if (oid != other.oid) return false;
    if (subid != other.subid) return false;
    return true;
  }

  bool operator!=(const ObjectModelReference& other) const
  {
    if (&other == this) return false;
    if (serverId != other.serverId) return true;
    if (database != other.database) return true;
    if (regclass != other.regclass) return true;
    if (oid != other.oid) return true;
    if (subid != other.subid) return true;
    return false;
  }

  static const Oid PG_ATTRIBUTE = 1249;
  static const Oid PG_CLASS = 1259;
  static const Oid PG_CONSTRAINT = 2606;
  static const Oid PG_DATABASE = 1262;
  static const Oid PG_EXTENSION = 3079;
  static const Oid PG_INDEX = 2610;
  static const Oid PG_NAMESPACE = 2615;
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
  Oid oid;
};

/**
 * A check constraint associated with a table.
 */
class CheckConstraintModel : public ObjectModel {
public:
  wxString expression;
  Oid oid;
};

/**
 * Some object that is a direct database member.
 */
class DatabaseMemberModel : public ObjectModel {
public:
  Oid oid;
};

/**
 * A namespace within the database.
 */
class SchemaModel : public DatabaseMemberModel {
public:
  bool IsSystem() const
  {
    return name.StartsWith(_T("pg_")) || name == _T("information_schema");
  }
};

/**
 * An extension within the database.
 */
class ExtensionModel : public DatabaseMemberModel {
public:
  bool operator<(const ExtensionModel& other) const
  {
    return oid < other.oid;
  }
};

/**
 * A schema member - function or relation.
 */
class SchemaMemberModel : public ObjectModel {
public:
  Oid oid;
  SchemaModel schema;
  ExtensionModel extension;
  wxString FormatName() const { return QualifiedName(); }
  wxString QualifiedName() const { return schema.name + _T('.') + name; }
  wxString UnqualifiedName() const { return name; }
  bool IsSystem() const { return schema.IsSystem(); }
  bool IsUser() const { return !schema.IsSystem(); }
  static bool CollateByQualifiedName(const SchemaMemberModel *r1, const SchemaMemberModel *r2) {
    if (r1->schema.name < r2->schema.name) return true;
    if (r1->schema.name == r2->schema.name) {
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
  std::vector<ColumnModel> columns;
  std::vector<IndexModel> indices;
  std::vector<TriggerModel> triggers;
  std::vector<RelationModel> sequences;
  std::vector<CheckConstraintModel> checkConstraints;
};

/**
 * A function.
 */
class FunctionModel : public SchemaMemberModel {
public:
  wxString arguments;
  enum Type { SCALAR, RECORDSET, TRIGGER, AGGREGATE, WINDOW } type;
  wxString FormatName() const { return schema.name + _T('.') + name + _T('(') + arguments + _T(')'); }
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
 * Some object that is a direct server member.
 */
class ServerMemberModel : public ObjectModel {
public:
  Oid oid;
};

/**
 * A tablespace.
 */
class TablespaceModel : public ServerMemberModel {
public:
  wxString location;
  bool IsSystem() const { return name.StartsWith(_T("pg_")); }
};

class ObjectBrowserManagedWork;

/**
 * Callback interface to notify some client that the schema index has been built.
 */
class IndexSchemaCompletionCallback {
public:
  virtual ~IndexSchemaCompletionCallback() {}
  /**
   * Called when schema index completed.
   */
  virtual void Completed(const CatalogueIndex& index) = 0;
};

/**
 * A database.
 *
 * This also contains a OID-to-tree-item lookup table and a CatalogueIndex.
 */
class DatabaseModel : public ServerMemberModel {
public:
  DatabaseModel() : loaded(false), server(NULL), catalogueIndex(NULL) {}
  virtual ~DatabaseModel() {
    if (catalogueIndex != NULL)
      delete catalogueIndex;
  }
  operator ObjectModelReference () const;
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
  ObjectModel *FindObject(const ObjectModelReference& ref);
  std::vector<RelationModel> relations;
  std::vector<FunctionModel> functions;
  std::vector<TextSearchDictionaryModel> textSearchDictionaries;
  std::vector<TextSearchParserModel> textSearchParsers;
  std::vector<TextSearchTemplateModel> textSearchTemplates;
  std::vector<TextSearchConfigurationModel> textSearchConfigurations;

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
    std::map<const ExtensionModel, std::vector<const SchemaMemberModel*> > extensionDivisions;
  };

  /**
   * Divide the schema members up into user, system and extension members.
   */
  Divisions DivideSchemaMembers() const;

  /**
   * Find a particular relation.
   */
  RelationModel *FindRelation(const ObjectModelReference &ref)
  {
    wxASSERT(ref.GetObjectClass() == ObjectModelReference::PG_CLASS);
    ObjectModel *obj = FindObject(ref);
    if (obj == NULL) return NULL;
    return static_cast<RelationModel*>(obj);
  }

  /**
   * Find a particular function.
   */
  FunctionModel *FindFunction(const ObjectModelReference &ref)
  {
    wxASSERT(ref.GetObjectClass() == ObjectModelReference::PG_PROC);
    ObjectModel *obj = FindObject(ref);
    if (obj == NULL) return NULL;
    return static_cast<FunctionModel*>(obj);
  }

  /**
   * Load the database schema.
   */
  void Load(IndexSchemaCompletionCallback *indexCompletion = NULL);

  /**
   * Load the detail of a particular relation.
   */
  void LoadRelation(const ObjectModelReference& ref);

  /**
   * Submit some work to this database.
   */
  void SubmitWork(ObjectBrowserManagedWork *work);
};

/**
 * A server role.
 */
class RoleModel : public ServerMemberModel {
public:
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
  void UpdateServerParameters(const wxString& serverVersionString_, int serverVersion_, SSL *ssl);

  void UpdateDatabases(const std::vector<DatabaseModel>& incoming);
  void UpdateRoles(const std::vector<RoleModel>& incoming);
  void UpdateTablespaces(const std::vector<TablespaceModel>& incoming);

  void UpdateDatabase(const DatabaseModel& incoming);

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
   * Gets a reference to the administrative database.
   */
  ObjectModelReference GetServerAdminDatabaseRef() const
  {
    // this is a nasty hack to allow returning the reference to the admin db when we may not know its OID yet.
    return ObjectModelReference(Identification(), InvalidOid);
  }
  /**
   * Find the administrative database model on this server.
   */
  DatabaseModel *FindAdminDatabase() { return FindDatabase(GlobalDbName()); }
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

  /**
   * Clean up expired and stale database connections.
   */
  void Maintain();

  /**
   * Load database list.
   */
  void Load();

  /**
   * Submit some work to this server.
   */
  void SubmitWork(ObjectBrowserManagedWork*);
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
  ServerModel *FindServer(const ServerConnection &server) { return FindServerById(server.Identification()); }
  /**
   * Find an existing server by reference.
   */
  ServerModel *FindServer(const ObjectModelReference &ref) { wxASSERT(ref.GetOid() == InvalidOid); return FindServerById(ref.GetServerId()); }

  /**
   * Find an existing database matching some connection parameters and database name.
   */
  DatabaseModel *FindDatabase(const ServerConnection &server, const wxString &dbname);
  /**
   * Find an existing database by reference.
   */
  DatabaseModel *FindDatabase(const ObjectModelReference &ref);
  /**
   * Find the administrative database for a server by reference.
   */
  DatabaseModel *FindAdminDatabase(const ObjectModelReference &ref) { wxASSERT(ref.GetOid() == InvalidOid); return FindAdminDatabase(ref.GetServerId()); }
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
  void RemoveServer(const wxString& serverId);
  void SetupDatabaseConnection(const ObjectModelReference& ref, DatabaseConnection *db);

  /**
   * Find any object by reference.
   */
  ObjectModel *FindObject(const ObjectModelReference &ref);

  /**
   * Find a particular relation.
   */
  RelationModel *FindRelation(const ObjectModelReference &ref)
  {
    wxASSERT(ref.GetObjectClass() == ObjectModelReference::PG_CLASS);
    ObjectModel *obj = FindObject(ref);
    if (obj == NULL) return NULL;
    return static_cast<RelationModel*>(obj);
  }

  /**
   * Find a particular function.
   */
  FunctionModel *FindFunction(const ObjectModelReference &ref)
  {
    wxASSERT(ref.GetObjectClass() == ObjectModelReference::PG_PROC);
    ObjectModel *obj = FindObject(ref);
    if (obj == NULL) return NULL;
    return static_cast<FunctionModel*>(obj);
  }

  /**
   * Register a view with the model.
   *
   * The view will receive update notifications as the model updates.
   */
  void RegisterView(ObjectBrowser *view)
  {
    views.push_back(view);
  }

  /**
   * Unregister a view with the model.
   */
  void UnregisterView(ObjectBrowser *view)
  {
    views.remove(view);
  }

  /**
   * Clean up expired and stale database connections.
   */
  void Maintain();

  static const int TIMER_MAINTAIN = 20000;
private:
  ServerModel *FindServerById(const wxString&);
  DatabaseModel *FindAdminDatabase(const wxString&);
  std::list<ServerModel> servers;
  std::list<ObjectBrowser*> views;
  DECLARE_EVENT_TABLE();
  void OnWorkFinished(wxCommandEvent&);
  void OnWorkCrashed(wxCommandEvent&);
  void OnRescheduleWork(wxCommandEvent&);
  void OnTimerTick(wxTimerEvent&);
  void SubmitServerWork(ServerModel*, ObjectBrowserManagedWork*);
  void SubmitDatabaseWork(DatabaseModel*, ObjectBrowserManagedWork*);
  void ConnectAndAddWork(const ObjectModelReference& ref, DatabaseConnection *db, DatabaseWork *work);

  friend class ServerModel;
  friend class DatabaseModel;
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
