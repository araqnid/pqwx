/**
 * @file
 * Database interaction performed by the object browser
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_database_work_h
#define __object_browser_database_work_h

#include "wx/msgdlg.h"
#include "object_browser.h"
#include "object_browser_model.h"
#include "query_results.h"
#include "database_work.h"
#include "script_events.h"

/**
 * Unit of work to be performed on a database connection for the
 * object browser.
 *
 * An instance of this class is wrapped by a DatabaseWork in order to
 * be executed. However, the DatabaseWork is deleted as soon as it has
 * completed, but the wrapper will pass this class back to object
 * browser as part of a "work finished" event so that is is available
 * in the GUI thread.
 */
class ObjectBrowserWork {
public:
  ObjectBrowserWork(const VersionedSql &sqlDictionary = ObjectBrowser::GetSqlDictionary()) : sqlDictionary(sqlDictionary) {}

  /**
   * Execute work.
   *
   * Similar to the same-named method in DatabaseWork. This method is
   * executed on the database worker thread.
   */
  virtual void operator()() = 0;
  /**
   * Merge data obtained by work object into object browser.
   *
   * This method is executed on the GUI thread.
   */
  virtual void LoadIntoView(ObjectBrowser *browser) = 0;
protected:
  DatabaseWorkWithDictionary::NamedQueryExecutor Query(const wxString &name)
  {
    return owner->Query(name);
  }

  /**
   * The actual libpq connection object.
   */
  PGconn *conn;
public:
  /**
   * Quote an identified for use in a generated SQL statement.
   */
  wxString QuoteIdent(const wxString &value) { return owner->QuoteIdent(value); }
  /**
   * Quote an literal string for use in a generated SQL statement.
   */
  wxString QuoteLiteral(const wxString &value) { return owner->QuoteLiteral(value); }
  /**
   * The actual database work object wrapping this.
   */
  DatabaseWorkWithDictionary *owner;

private:
  const VersionedSql& sqlDictionary;

  friend class ObjectBrowserDatabaseWork;
};

/**
 * Implementation of DatabaseWork that deals with passing results back to the GUI thread.
 */
class ObjectBrowserDatabaseWork : public DatabaseWorkWithDictionary {
public:
  /**
   * Create work object.
   */
  ObjectBrowserDatabaseWork(wxEvtHandler *dest, ObjectBrowserWork *work) : DatabaseWorkWithDictionary(work->sqlDictionary), dest(dest), work(work) {}
  void operator()() {
    TransactionBoundary txn(this);
    work->owner = this;
    work->conn = conn;
    (*work)();
  }
  void NotifyFinished() {
    wxCommandEvent event(PQWX_ObjectBrowserWorkFinished);
    event.SetClientData(work);
    dest->AddPendingEvent(event);
  }

private:
  wxEvtHandler *dest;
  ObjectBrowserWork *work;

  class TransactionBoundary {
  public:
    TransactionBoundary(DatabaseWork *work) : work(work), began(FALSE)
    {
      work->DoCommand("BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY");
      began = TRUE;
    }
    ~TransactionBoundary()
    {
      if (began) {
	try {
	  work->DoCommand("END");
	} catch (...) {
	  // ignore exceptions trying to end transaction
	}
      }
    }
  private:
    DatabaseWork *work;
    bool began;
  };
};

/**
 * Initialise database connection.
 *
 * Run once on each database connection added to the object browser.
 */
class SetupDatabaseConnectionWork : public ObjectBrowserWork {
protected:
  void operator()() {
    owner->DoCommand(_T("SetupObjectBrowserConnection"));
  }
  void LoadIntoView(ObjectBrowser *ob) {
  }
};

/**
 * Load server information, database list and role list from server.
 */
class RefreshDatabaseListWork : public ObjectBrowserWork {
public:
  /**
   * Create work object
   * @param serverModel Server model to populate
   * @param serverItem Server tree item to populate
   */
  RefreshDatabaseListWork(ServerModel *serverModel, wxTreeItemId serverItem) : serverModel(serverModel), serverItem(serverItem) {
    wxLogDebug(_T("%p: work to load database list"), this);
  }
protected:
  void operator()() {
    ReadServer();
    ReadDatabases();
    ReadRoles();
  }
  void LoadIntoView(ObjectBrowser *ob) {
    ob->FillInServer(serverModel, serverItem);
    ob->EnsureVisible(serverItem);
    ob->SelectItem(serverItem);
    ob->Expand(serverItem);
  }
private:
  ServerModel *serverModel;
  wxTreeItemId serverItem;
  wxString serverVersionString;
  int serverVersion;
  bool usingSSL;
  void ReadServer() {
    serverModel->ReadServerParameters(PQparameterStatus(conn, "server_version"), PQserverVersion(conn), PQgetssl(conn));
  }

  static bool CollateDatabases(DatabaseModel *d1, DatabaseModel *d2) {
    return d1->name < d2->name;
  }

  void ReadDatabases() {
    QueryResults databaseRows = Query(_T("Databases")).List();
    for (QueryResults::const_iterator iter = databaseRows.begin(); iter != databaseRows.end(); iter++) {
      DatabaseModel *database = new DatabaseModel();
      database->server = serverModel;
      database->loaded = false;
      database->oid = (*iter).ReadOid(0);
      database->name = (*iter).ReadText(1);
      database->isTemplate = (*iter).ReadBool(2);
      database->allowConnections = (*iter).ReadBool(3);
      database->havePrivsToConnect = (*iter).ReadBool(4);
      database->description = (*iter).ReadText(5);
      serverModel->databases.push_back(database);
    }
    sort(serverModel->databases.begin(), serverModel->databases.end(), CollateDatabases);
  }

  static bool CollateRoles(RoleModel *r1, RoleModel *r2) {
    return r1->name < r2->name;
  }

  void ReadRoles() {
    QueryResults roleRows = Query(_T("Roles")).List();
    for (QueryResults::const_iterator iter = roleRows.begin(); iter != roleRows.end(); iter++) {
      RoleModel *role = new RoleModel();
      role->oid = (*iter).ReadOid(0);
      role->name = (*iter).ReadText(1);
      role->canLogin = (*iter).ReadBool(2);
      role->superuser = (*iter).ReadBool(3);
      role->description = (*iter).ReadText(4);
      serverModel->roles.push_back(role);
    }
    sort(serverModel->roles.begin(), serverModel->roles.end(), CollateRoles);
  }
};

/**
 * Load schema members (relations and functions) for a database.
 */
class LoadDatabaseSchemaWork : public ObjectBrowserWork {
public:
  /**
   * Create work object.
   * @param databaseModel Database model to populate
   * @param databaseItem Database tree item to populate
   * @param expandAfter Expand tree item after populating
   */
  LoadDatabaseSchemaWork(DatabaseModel *databaseModel, wxTreeItemId databaseItem, bool expandAfter) : databaseModel(databaseModel), databaseItem(databaseItem), expandAfter(expandAfter) {
    wxLogDebug(_T("%p: work to load schema"), this);
  }
private:
  DatabaseModel *databaseModel;
  wxTreeItemId databaseItem;
  bool expandAfter;
protected:
  void operator()() {
    LoadRelations();
    LoadFunctions();
  }
  void LoadRelations() {
    QueryResults relationRows = Query(_T("Relations")).List();
    for (QueryResults::const_iterator iter = relationRows.begin(); iter != relationRows.end(); iter++) {
      RelationModel *relation = new RelationModel();
      relation->database = databaseModel;
      relation->schema = (*iter).ReadText(1);
      relation->user = !IsSystemSchema(relation->schema);
      if (!(*iter)[0].IsEmpty()) {
	relation->oid = (*iter).ReadOid(0);
	relation->name = (*iter).ReadText(2);
	wxString relkind((*iter).ReadText(3));
	relation->extension = (*iter).ReadText(4);
	wxASSERT_MSG(relationTypeMap.count(relkind) > 0, relkind);
	relation->type = relationTypeMap.find(relkind)->second;
      }
      databaseModel->relations.push_back(relation);
    }
  }
  void LoadFunctions() {
    QueryResults functionRows = Query(_T("Functions")).List();
    for (QueryResults::const_iterator iter = functionRows.begin(); iter != functionRows.end(); iter++) {
      FunctionModel *func = new FunctionModel();
      func->database = databaseModel;
      func->oid = (*iter).ReadOid(0);
      func->schema = (*iter).ReadText(1);
      func->name = (*iter).ReadText(2);
      func->arguments = (*iter).ReadText(3);
      wxString type((*iter).ReadText(4));
      func->extension = (*iter).ReadText(5);
      wxASSERT_MSG(functionTypeMap.count(type) > 0, type);
      func->type = functionTypeMap.find(type)->second;
      func->user = !IsSystemSchema(func->schema);
      databaseModel->functions.push_back(func);
    }
  }
  void LoadIntoView(ObjectBrowser *ob) {
    ob->FillInDatabaseSchema(databaseModel, databaseItem);
    if (expandAfter) ob->Expand(databaseItem);
    ob->SetItemText(databaseItem, databaseModel->name); // remove loading message
  }
private:
  static inline bool IsSystemSchema(wxString schema) {
    return schema.StartsWith(_T("pg_")) || schema == _T("information_schema");
  }
  static const std::map<wxString, RelationModel::Type> relationTypeMap;
  static const std::map<wxString, FunctionModel::Type> functionTypeMap;
};

/**
 * Load descriptions of objects from database.
 */
class LoadDatabaseDescriptionsWork : public ObjectBrowserWork {
public:
  /**
   * @param databaseModel Database model to populate
   */
  LoadDatabaseDescriptionsWork(DatabaseModel *databaseModel) : databaseModel(databaseModel) {
    wxLogDebug(_T("%p: work to load schema object descriptions"), this);
  }
private:
  DatabaseModel *databaseModel;
  std::map<unsigned long, wxString> descriptions;
protected:
  void operator()() {
    QueryResults rs = Query(_T("Object Descriptions")).List();
    for (QueryResults::const_iterator iter = rs.begin(); iter != rs.end(); iter++) {
      unsigned long oid;
      wxString description;
      oid = (*iter).ReadOid(0);
      description = (*iter).ReadText(1);
      descriptions[oid] = description;
    }
  }
  void LoadIntoView(ObjectBrowser *ob) {
    int count = 0;
    for (std::vector<RelationModel*>::iterator iter = databaseModel->relations.begin(); iter != databaseModel->relations.end(); iter++) {
      std::map<unsigned long, wxString>::const_iterator ptr = descriptions.find((*iter)->oid);
      if (ptr != descriptions.end()) {
	(*iter)->description = (*ptr).second;
	++count;
      }
    }
    for (std::vector<FunctionModel*>::iterator iter = databaseModel->functions.begin(); iter != databaseModel->functions.end(); iter++) {
      std::map<unsigned long, wxString>::const_iterator ptr = descriptions.find((*iter)->oid);
      if (ptr != descriptions.end()) {
	(*iter)->description = (*ptr).second;
	++count;
      }
    }
    wxLogDebug(_T("Loaded %d/%lu descriptions"), count, descriptions.size());
  }
};

/**
 * Callback interface to notify some client that the schema index has been built.
 */
class IndexSchemaCompletionCallback {
public:
  /**
   * Called when schema index completed.
   */
  virtual void Completed(ObjectBrowser *ob, DatabaseModel *db, const CatalogueIndex *index) = 0;
};

/**
 * Build index of database schema for object finder.
 */
class IndexDatabaseSchemaWork : public ObjectBrowserWork {
public:
  /**
   * @param database Database being indexed
   * @param completion Additional callback to notify when indexing completed
   */
  IndexDatabaseSchemaWork(DatabaseModel *database, IndexSchemaCompletionCallback *completion = NULL) : database(database), completion(completion) {
    wxLogDebug(_T("%p: work to index schema"), this);
  }
private:
  DatabaseModel *database;
  IndexSchemaCompletionCallback *completion;
  CatalogueIndex *catalogueIndex;
  static const std::map<wxString, CatalogueIndex::Type> typeMap;
protected:
  void operator()() {
    QueryResults rs = Query(_T("IndexSchema")).List();
    catalogueIndex = new CatalogueIndex();
    catalogueIndex->Begin();
    for (QueryResults::const_iterator iter = rs.begin(); iter != rs.end(); iter++) {
      Oid entityId = (*iter).ReadOid(0);
      wxString typeString = (*iter).ReadText(1);
      wxString symbol = (*iter).ReadText(2);
      wxString disambig = (*iter).ReadText(3);
      bool systemObject;
      CatalogueIndex::Type entityType;
      if (typeString.Last() == _T('S')) {
	systemObject = true;
	typeString.RemoveLast();
	wxASSERT(typeMap.count(typeString) > 0);
	entityType = typeMap.find(typeString)->second;
      }
      else {
	systemObject = false;
	wxASSERT(typeMap.count(typeString) > 0);
	entityType = typeMap.find(typeString)->second;
      }
      catalogueIndex->AddDocument(CatalogueIndex::Document(entityId, entityType, systemObject, symbol, disambig));
    }
    catalogueIndex->Commit();
  }
  void LoadIntoView(ObjectBrowser *ob) {
    database->catalogueIndex = catalogueIndex;
    if (completion) {
      completion->Completed(ob, database, catalogueIndex);
      delete completion;
    }
  }
};

/**
 * Load relation details from database
 */
class LoadRelationWork : public ObjectBrowserWork {
public:
  /**
   * @param relationModel Relation model to populate
   * @param relationItem Relation tree item to populate
   */
  LoadRelationWork(RelationModel *relationModel, wxTreeItemId relationItem) : relationModel(relationModel), relationItem(relationItem) {
    wxLogDebug(_T("%p: work to load relation"), this);
  }
private:
  RelationModel *relationModel;
  wxTreeItemId relationItem;
  std::vector<ColumnModel*> columns;
  std::vector<IndexModel*> indices;
  std::vector<TriggerModel*> triggers;
  std::vector<RelationModel*> sequences;
protected:
  void operator()() {
    ReadColumns();
    if (relationModel->type == RelationModel::TABLE) {
      ReadIndices();
      ReadTriggers();
      ReadSequences();
    }
  }
private:
  void ReadColumns() {
    QueryResults attributeRows = Query(_T("Columns")).OidParam(relationModel->oid).List();
    for (QueryResults::const_iterator iter = attributeRows.begin(); iter != attributeRows.end(); iter++) {
      ColumnModel *column = new ColumnModel();
      column->relation = relationModel;
      column->name = (*iter).ReadText(0);
      column->type = (*iter).ReadText(1);
      column->nullable = (*iter).ReadBool(2);
      column->hasDefault = (*iter).ReadBool(3);
      column->description = (*iter).ReadText(4);
      column->attnum = (*iter).ReadInt4(5);
      columns.push_back(column);
    }
  }
  void ReadIndices() {
    QueryResults indexRows = Query(_T("Indices")).OidParam(relationModel->oid).List();
    IndexModel *lastIndex = NULL;
    for (QueryResults::const_iterator iter = indexRows.begin(); iter != indexRows.end(); iter++) {
      IndexModel *index;
      wxString indexName = (*iter)[_T("relname")];
      if (lastIndex == NULL || lastIndex->name != indexName) {
	index = new IndexModel();
	index->name = indexName;
	lastIndex = index;
	indices.push_back(index);
      }
      else {
	index = lastIndex;
      }
      index->columns.push_back((*iter)[_T("attname")]);
    }
  }
  void ReadTriggers() {
    QueryResults triggerRows = Query(_T("Triggers")).OidParam(relationModel->oid).List();
    for (QueryResults::const_iterator iter = triggerRows.begin(); iter != triggerRows.end(); iter++) {
      TriggerModel *trigger = new TriggerModel();
      trigger->name = (*iter).ReadText(0);
      triggers.push_back(trigger);
    }
  }
  void ReadSequences() {
    QueryResults sequenceRows = Query(_T("Sequences")).OidParam(relationModel->oid).List();
    for (QueryResults::const_iterator iter = sequenceRows.begin(); iter != sequenceRows.end(); iter++) {
      RelationModel *sequence = new RelationModel();
      sequence->type = RelationModel::SEQUENCE;
      sequence->database = relationModel->database;
      sequence->oid = (*iter).ReadOid(0);
      sequence->schema = (*iter).ReadText(1);
      sequence->name = (*iter).ReadText(2);
      sequence->owningColumn = (*iter).ReadInt4(3);
      sequences.push_back(sequence);
    }
  }
protected:
  void LoadIntoView(ObjectBrowser *ob) {
    ob->FillInRelation(relationModel, relationItem, columns, indices, triggers, sequences);
    ob->Expand(relationItem);

    // remove 'loading...' tag
    wxString itemText = ob->GetItemText(relationItem);
    int space = itemText.Find(_T(' '));
    if (space != wxNOT_FOUND) {
      ob->SetItemText(relationItem, itemText.Left(space));
    }
  }
};

#endif

// Local Variables:
// mode: c++
// End:
