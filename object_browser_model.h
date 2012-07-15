/**
 * @file
 * Model classes for the object browser
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_model_h
#define __object_browser_model_h

#include "catalogue_index.h"
#include "database_connection.h"
#include "object_model_reference.h"

class RelationModel;
class DatabaseModel;
class ServerModel;
class ObjectBrowser;

class PQWXObjectBrowserModelEvent : public wxNotifyEvent {
public:
  PQWXObjectBrowserModelEvent(const ObjectModelReference& ref, wxEventType type = wxEVT_NULL, int id = 0) : wxNotifyEvent(type, id), ref(ref) {}

  const ObjectModelReference ref;
};

/**
 * Prototype for handling a database event.
 */
typedef void (wxEvtHandler::*PQWXObjectBrowserModelEventFunction)(PQWXObjectBrowserModelEvent&);

/**
 * Static event table macro.
 */
#define EVT_OBJECT_BROWSER_MODEL(id, type, fn) \
    DECLARE_EVENT_TABLE_ENTRY( type, id, -1, \
    (wxObjectEventFunction) (wxEventFunction) (PQWXObjectBrowserModelEventFunction) (wxNotifyEventFunction) \
    wxStaticCastEvent( PQWXObjectBrowserModelEventFunction, & fn ), (wxObject *) NULL ),

BEGIN_DECLARE_EVENT_TYPES()
  DECLARE_EVENT_TYPE(PQWX_RescheduleObjectBrowserWork, -1)
  DECLARE_EVENT_TYPE(PQWX_ObjectBrowserConnectionMade, -1)
  DECLARE_EVENT_TYPE(PQWX_ObjectBrowserConnectionFailed, -1)
  DECLARE_EVENT_TYPE(PQWX_ObjectBrowserConnectionNeedsPassword, -1)
END_DECLARE_EVENT_TYPES()

#define PQWX_OBJECT_BROWSER_CONNECTION_MADE(id, fn) EVT_OBJECT_BROWSER_MODEL(id, PQWX_ObjectBrowserConnectionMade, fn)
#define PQWX_OBJECT_BROWSER_CONNECTION_FAILED(id, fn) EVT_OBJECT_BROWSER_MODEL(id, PQWX_ObjectBrowserConnectionFailed, fn)
#define PQWX_OBJECT_BROWSER_CONNECTION_NEEDS_PASSWORD(id, fn) EVT_OBJECT_BROWSER_MODEL(id, PQWX_ObjectBrowserConnectionNeedsPassword, fn)

/**
 * Event issued when asynchronous database work needs rescheduling after a connection was lost
 */
#define PQWX_RESCHEDULE_OBJECT_BROWSER_WORK(id, fn) EVT_COMMAND(id, PQWX_RescheduleObjectBrowserWork, fn)

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
   * Drop database.
   */
  void Drop();

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

class SSLInfo;

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
  ServerModel(const ServerConnection &conninfo) : conninfo(conninfo), sslInfo(NULL) {}
  /**
   * Create server model with an initial connection.
   */
  ServerModel(const ServerConnection &conninfo, DatabaseConnection *db) : conninfo(conninfo), sslInfo(NULL) {
    connections[db->DbName()] = db;
  }
  ~ServerModel();

  /**
   * Initialise some server parameters.
   *
   * @param serverVersionString_ Server version as string such as "9.1.1"
   * @param serverVersion_ Server version as an integer such as 90101
   * @param ssl SSL object (NULL if not SSL)
   */
  void UpdateServerParameters(const wxString& serverVersionString_, int serverVersion_, SSLInfo* sslInfo_);

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
  bool IsUsingSSL() const { return sslInfo != NULL; }
  /**
   * @return SSL information from the initial connection
   */
  const SSLInfo& GetSSLInfo() const { return *sslInfo; }
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

  bool HaveCreateDBPrivilege() const { return haveCreateDBPrivilege; }
  bool HaveCreateUserPrivilege() const { return haveCreateUserPrivilege; }
  bool HaveSuperuserStatus() const { return haveSuperuserStatus; }

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
  bool haveCreateDBPrivilege;
  bool haveCreateUserPrivilege;
  bool haveSuperuserStatus;
  int serverVersion;
  wxString serverVersionString;
  SSLInfo *sslInfo;
  std::map<wxString, DatabaseConnection*> connections;
  DatabaseModel *FindDatabaseByOid(Oid oid);
  void DropDatabase(DatabaseModel*);
  void RemoveDatabase(const wxString& dbname);
  friend class RefreshDatabaseListWork;
  friend class DatabaseModel;
  friend class DropDatabaseWork;
};

inline void DatabaseModel::Drop() { server->DropDatabase(this); }

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
  void OnConnectionMade(PQWXObjectBrowserModelEvent&);
  void OnConnectionFailed(PQWXObjectBrowserModelEvent&);
  void OnConnectionNeedsPassword(PQWXObjectBrowserModelEvent&);
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
