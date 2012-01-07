/**
 * @file
 * Database interaction performed by the object browser
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_database_work_h
#define __object_browser_database_work_h

#include "wx/msgdlg.h"
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
  /**
   * Execute work.
   *
   * Similar to the same-named method in DatabaseWork. This method is
   * executed on the database worker thread.
   */
  virtual void Execute() = 0;
  /**
   * Merge data obtained by work object into object browser.
   *
   * This method is executed on the GUI thread.
   */
  virtual void LoadIntoView(ObjectBrowser *browser) = 0;
protected:
  /**
   * Perform a query with a single numeric parameter and return a result set.
   */
  QueryResults DoQuery(const wxString &name, Oid paramType, unsigned long paramValue) {
    wxString valueString = wxString::Format(_T("%lu"), paramValue);
    return owner->DoNamedQuery(name, paramType, valueString.utf8_str());
  }
  /**
   * Perform a query with a single numerc parameter and return a single row.
   */
  QueryResults::Row DoSingleRowQuery(const wxString &name, Oid paramType, unsigned long paramValue) {
    wxString valueString = wxString::Format(_T("%lu"), paramValue);
    QueryResults results = owner->DoNamedQuery(name, paramType, valueString.utf8_str());
    wxASSERT(results.size() == 1);
    return results[0];
  }
  /**
   * Perform a query with a single string parameter and return a result set.
   */
  QueryResults DoQuery(const wxString &name, Oid paramType, const char *paramValue) {
    return owner->DoNamedQuery(name, paramType, paramValue);
  }
  /**
   * Perform a query with a no parameters and return a result set.
   */
  QueryResults DoQuery(const wxString &name) {
    return owner->DoNamedQuery(name);
  }
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
  DatabaseWork *owner;
  /**
   * The actual libpq connection object.
   */
  PGconn *conn;
  friend class ObjectBrowserDatabaseWork;
};

/**
 * Implementation of DatabaseWork that deals with passing results back to the GUI thread.
 */
class ObjectBrowserDatabaseWork : public DatabaseWork {
public:
  /**
   * State of this work object.
   */
  enum State { PENDING, EXECUTED, NOTIFIED };
  /**
   * Create work object.
   */
  ObjectBrowserDatabaseWork(wxEvtHandler *dest, ObjectBrowserWork *work) : DatabaseWork(&(ObjectBrowser::GetSqlDictionary())), dest(dest), work(work), state(PENDING) {}
  void Execute() {
    ChangeState(PENDING, EXECUTED);
    work->owner = this;
    work->conn = conn;
    work->Execute();
    state = EXECUTED;
  }
  void NotifyFinished() {
    ChangeState(EXECUTED, NOTIFIED);
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, EVENT_WORK_FINISHED);
    event.SetClientData(work);
    dest->AddPendingEvent(event);
  }
  /**
   * Get the current state of this work object.
   */
  State GetState() const {
    wxCriticalSectionLocker locker(crit);
    return state;
  }
private:
  wxEvtHandler *dest;
  ObjectBrowserWork *work;
  mutable wxCriticalSection crit;
  State state;
  void ChangeState(State oldState, State newState) {
    wxCriticalSectionLocker locker(crit);
    wxASSERT(state == oldState);
    state = newState;
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
  void Execute() {
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
    QueryResults databaseRows = DoQuery(_T("Databases"));
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
    QueryResults roleRows = DoQuery(_T("Roles"));
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
  void Execute() {
    LoadRelations();
    LoadFunctions();
  }
  void LoadRelations() {
    std::map<wxString, RelationModel::Type> typemap;
    typemap[_T("r")] = RelationModel::TABLE;
    typemap[_T("v")] = RelationModel::VIEW;
    typemap[_T("S")] = RelationModel::SEQUENCE;
    QueryResults relationRows = DoQuery(_T("Relations"));
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
	relation->type = typemap[relkind];
      }
      databaseModel->relations.push_back(relation);
    }
  }
  void LoadFunctions() {
    std::map<wxString, FunctionModel::Type> typemap;
    typemap[_T("f")] = FunctionModel::SCALAR;
    typemap[_T("ft")] = FunctionModel::TRIGGER;
    typemap[_T("fs")] = FunctionModel::RECORDSET;
    typemap[_T("fa")] = FunctionModel::AGGREGATE;
    typemap[_T("fw")] = FunctionModel::WINDOW;
    QueryResults functionRows = DoQuery(_T("Functions"));
    for (QueryResults::const_iterator iter = functionRows.begin(); iter != functionRows.end(); iter++) {
      FunctionModel *func = new FunctionModel();
      func->database = databaseModel;
      func->oid = (*iter).ReadOid(0);
      func->schema = (*iter).ReadText(1);
      func->name = (*iter).ReadText(2);
      func->arguments = (*iter).ReadText(3);
      wxString type((*iter).ReadText(4));
      func->extension = (*iter).ReadText(5);
      wxASSERT_MSG(typemap.find(type) != typemap.end(), type);
      func->type = typemap[type];
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
  void Execute() {
    QueryResults rs = DoQuery(_T("Object Descriptions"));
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
protected:
  void Execute() {
    std::map<wxString, CatalogueIndex::Type> typeMap;
    typeMap[_T("t")] = CatalogueIndex::TABLE;
    typeMap[_T("v")] = CatalogueIndex::VIEW;
    typeMap[_T("s")] = CatalogueIndex::SEQUENCE;
    typeMap[_T("f")] = CatalogueIndex::FUNCTION_SCALAR;
    typeMap[_T("fs")] = CatalogueIndex::FUNCTION_ROWSET;
    typeMap[_T("ft")] = CatalogueIndex::FUNCTION_TRIGGER;
    typeMap[_T("fa")] = CatalogueIndex::FUNCTION_AGGREGATE;
    typeMap[_T("fw")] = CatalogueIndex::FUNCTION_WINDOW;
    typeMap[_T("T")] = CatalogueIndex::TYPE;
    typeMap[_T("x")] = CatalogueIndex::EXTENSION;
    typeMap[_T("O")] = CatalogueIndex::COLLATION;
    QueryResults rs = DoQuery(_T("IndexSchema"));
    catalogueIndex = new CatalogueIndex();
    catalogueIndex->Begin();
    for (QueryResults::const_iterator iter = rs.begin(); iter != rs.end(); iter++) {
      Oid entityId;
      wxString typeString;
      wxString symbol;
      wxString disambig;
      entityId = (*iter).ReadOid(0);
      typeString = (*iter).ReadText(1);
      symbol = (*iter).ReadText(2);
      disambig = (*iter).ReadText(3);
      bool systemObject;
      CatalogueIndex::Type entityType;
      if (typeString.Last() == _T('S')) {
	systemObject = true;
	typeString.RemoveLast();
	wxASSERT(typeMap.count(typeString) > 0);
	entityType = typeMap[typeString];
      }
      else {
	systemObject = false;
	wxASSERT(typeMap.count(typeString) > 0);
	entityType = typeMap[typeString];
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
protected:
  void Execute() {
    ReadColumns();
    if (relationModel->type == RelationModel::TABLE) {
      ReadIndices();
      ReadTriggers();
    }
  }
private:
  void ReadColumns() {
    wxString oidValue;
    oidValue.Printf(_T("%d"), relationModel->oid);
    QueryResults attributeRows = DoQuery(_T("Columns"), 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::const_iterator iter = attributeRows.begin(); iter != attributeRows.end(); iter++) {
      ColumnModel *column = new ColumnModel();
      column->relation = relationModel;
      column->name = (*iter).ReadText(0);
      column->type = (*iter).ReadText(1);
      column->nullable = (*iter).ReadBool(2);
      column->hasDefault = (*iter).ReadBool(3);
      column->description = (*iter).ReadText(4);
      columns.push_back(column);
    }
  }
  void ReadIndices() {
    wxString oidValue = wxString::Format(_T("%d"), relationModel->oid);
    QueryResults indexRows = DoQuery(_T("Indices"), 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::const_iterator iter = indexRows.begin(); iter != indexRows.end(); iter++) {
      IndexModel *index = new IndexModel();
      index->name = (*iter).ReadText(0);
      indices.push_back(index);
    }
  }
  void ReadTriggers() {
    wxString oidValue = wxString::Format(_T("%d"), relationModel->oid);
    QueryResults triggerRows = DoQuery(_T("Triggers"), 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::const_iterator iter = triggerRows.begin(); iter != triggerRows.end(); iter++) {
      TriggerModel *trigger = new TriggerModel();
      trigger->name = (*iter).ReadText(0);
      triggers.push_back(trigger);
    }
  }
protected:
  void LoadIntoView(ObjectBrowser *ob) {
    ob->FillInRelation(relationModel, relationItem, columns, indices, triggers);
    ob->Expand(relationItem);

    // remove 'loading...' tag
    wxString itemText = ob->GetItemText(relationItem);
    int space = itemText.Find(_T(' '));
    if (space != wxNOT_FOUND) {
      ob->SetItemText(relationItem, itemText.Left(space));
    }
  }
};

/**
 * Generate SQL script for some database item.
 *
 * The SQL script is generated in some mode (create, alter, etc) and
 * with a designated output. This class deals with dispatching
 * generated SQL to the output: subclasses must supply implementations
 * to produce the script by populating the statements member in an
 * Execute() implementation.
 */
class ScriptWork : public ObjectBrowserWork {
public:
  /**
   * Type of script to produce.
   */
  enum Mode { Create, Alter, Drop, Select, Insert, Update, Delete };
  /**
   * Output channel.
   */
  enum Output { Window, File, Clipboard };
  ScriptWork(DatabaseModel *database, Mode mode, Output output) : database(database), mode(mode), output(output) {}
protected:
  std::vector<wxString> statements;
  void LoadIntoView(ObjectBrowser *ob) {
    wxString message;
    switch (output) {
    case Window: {
      PQWXDatabaseEvent evt(database->server->conninfo, database->name, PQWX_ScriptToWindow);
      wxString script;
      for (std::vector<wxString>::iterator iter = statements.begin(); iter != statements.end(); iter++) {
	script << *iter << _T("\n\n");
      }
      evt.SetString(script);
      ob->ProcessEvent(evt);
      return;
    }
      break;
    case File:
      message << _T("TODO Send to file:\n\n");
      break;
    case Clipboard:
      message << _T("TODO Send to clipboard:\n\n");
      break;
    default:
      wxASSERT(false);
    }
    for (std::vector<wxString>::iterator iter = statements.begin(); iter != statements.end(); iter++) {
      message << *iter << _T("\n\n");
    }
    wxLogDebug(_T("%s"), message.c_str());
    wxMessageBox(message);
  }
  DatabaseModel *database;
  const Mode mode;
private:
  const Output output;
};

/**
 * Produce database scripts.
 */
class DatabaseScriptWork : public ScriptWork {
public:
  DatabaseScriptWork(DatabaseModel *database, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(database, mode, output) {
    wxLogDebug(_T("%p: work to generate database script"), this);
  }
protected:
  void Execute();
};

/**
 * Produce table scripts.
 */
class TableScriptWork : public ScriptWork {
public:
  TableScriptWork(RelationModel *table, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(table->database, mode, output), table(table) {
    wxLogDebug(_T("%p: work to generate table script"), this);
  }
private:
  RelationModel *table;
protected:
  void Execute();
};

/**
 * Produce view scripts.
 */
class ViewScriptWork : public ScriptWork {
public:
  ViewScriptWork(RelationModel *view, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(view->database, mode, output), view(view) {
    wxLogDebug(_T("%p: work to generate view script"), this);
  }
private:
  RelationModel *view;
protected:
  void Execute();
};

/**
 * Produce sequence scripts.
 */
class SequenceScriptWork : public ScriptWork {
public:
  SequenceScriptWork(RelationModel *sequence, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(sequence->database, mode, output), sequence(sequence) {
    wxLogDebug(_T("%p: work to generate sequence script"), this);
  }
private:
  RelationModel *sequence;
protected:
  void Execute();
};

/**
 * Produce function scripts.
 */
class FunctionScriptWork : public ScriptWork {
public:
  FunctionScriptWork(FunctionModel *function, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(function->database, mode, output), function(function) {
    wxLogDebug(_T("%p: work to generate function script"), this);
  }
private:
  FunctionModel *function;
protected:
  void Execute();
private:
  struct Typeinfo {
    wxString schema;
    wxString name;
    int arrayDepth;
  };
  std::map<Oid, Typeinfo> FetchTypes(const std::vector<Oid> &types1, const std::vector<Oid> &types2) {
    std::set<Oid> typeSet;
    for (std::vector<Oid>::const_iterator iter = types1.begin(); iter != types1.end(); iter++) {
      typeSet.insert(*iter);
    }
    for (std::vector<Oid>::const_iterator iter = types2.begin(); iter != types2.end(); iter++) {
      typeSet.insert(*iter);
    }
    return FetchTypes(typeSet);
  }
  std::map<Oid, Typeinfo> FetchTypes(const std::set<Oid> &types);
};

#endif

// Local Variables:
// mode: c++
// End:
