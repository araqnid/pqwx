// -*- mode: c++ -*-

#ifndef __object_browser_database_work_h
#define __object_browser_database_work_h

#include "wx/msgdlg.h"
#include "query_results.h"
#include "database_work.h"
#include "script_events.h"

class ObjectBrowserWork {
public:
  virtual void Execute() = 0;
  virtual void LoadIntoView(ObjectBrowser *browser) = 0;
protected:
  bool DoQuery(const wxString &name, QueryResults &results, Oid paramType, unsigned long paramValue) {
    wxString valueString = wxString::Format(_T("%lu"), paramValue);
    return owner->DoNamedQuery(name, results, paramType, valueString.utf8_str());
  }
  bool DoQuery(const wxString &name, std::vector<wxString> &row, Oid paramType, unsigned long paramValue) {
    wxString valueString = wxString::Format(_T("%lu"), paramValue);
    QueryResults results;
    if (!owner->DoNamedQuery(name, results, paramType, valueString.utf8_str())) return false;
    wxASSERT(results.size() == 1);
    row = results[0];
    return true;
  }
  bool DoQuery(const wxString &name, QueryResults &results, Oid paramType, const char *paramValue) {
    return owner->DoNamedQuery(name, results, paramType, paramValue);
  }
  bool DoQuery(const wxString &name, QueryResults &results) {
    return owner->DoNamedQuery(name, results);
  }
  wxString QuoteIdent(const wxString &value) { return owner->QuoteIdent(value); }
  wxString QuoteLiteral(const wxString &value) { return owner->QuoteLiteral(value); }
  DatabaseWork *owner;
  PGconn *conn;
  friend class ObjectBrowserDatabaseWork;
};

class ObjectBrowserDatabaseWork : public DatabaseWork {
public:
  enum State { PENDING, EXECUTED, NOTIFIED };
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

class RefreshDatabaseListWork : public ObjectBrowserWork {
public:
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
    ob->FillInDatabases(serverModel, serverItem, databases);
    ob->FillInRoles(serverModel, serverItem, roles);

    ob->EnsureVisible(serverItem);
    ob->SelectItem(serverItem);
    ob->Expand(serverItem);
  }
private:
  ServerModel *serverModel;
  wxTreeItemId serverItem;
  std::vector<DatabaseModel*> databases;
  std::vector<RoleModel*> roles;
  wxString serverVersionString;
  int serverVersion;
  bool usingSSL;
  void ReadServer() {
    serverModel->ReadServerParameters(PQparameterStatus(conn, "server_version"), PQserverVersion(conn), PQgetssl(conn));
  }
  void ReadDatabases() {
    QueryResults databaseRows;
    DoQuery(_T("Databases"), databaseRows);
    for (QueryResults::iterator iter = databaseRows.begin(); iter != databaseRows.end(); iter++) {
      DatabaseModel *database = new DatabaseModel();
      database->server = serverModel;
      database->loaded = false;
      database->oid = ReadOid(iter, 0);
      database->name = ReadText(iter, 1);
      database->isTemplate = ReadBool(iter, 2);
      database->allowConnections = ReadBool(iter, 3);
      database->havePrivsToConnect = ReadBool(iter, 4);
      database->description = ReadText(iter, 5);
      databases.push_back(database);
    }
  }
  void ReadRoles() {
    QueryResults roleRows;
    DoQuery(_T("Roles"), roleRows);
    for (QueryResults::iterator iter = roleRows.begin(); iter != roleRows.end(); iter++) {
      RoleModel *role = new RoleModel();
      role->oid = ReadOid(iter, 0);
      role->name = ReadText(iter, 1);
      role->canLogin = ReadBool(iter, 2);
      role->superuser = ReadBool(iter, 3);
      role->description = ReadText(iter, 4);
      roles.push_back(role);
    }
  }
};

class LoadDatabaseSchemaWork : public ObjectBrowserWork {
public:
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
    QueryResults relationRows;
    std::map<wxString, RelationModel::Type> typemap;
    typemap[_T("r")] = RelationModel::TABLE;
    typemap[_T("v")] = RelationModel::VIEW;
    typemap[_T("S")] = RelationModel::SEQUENCE;
    DoQuery(_T("Relations"), relationRows);
    for (QueryResults::iterator iter = relationRows.begin(); iter != relationRows.end(); iter++) {
      RelationModel *relation = new RelationModel();
      relation->database = databaseModel;
      relation->schema = ReadText(iter, 1);
      relation->user = !IsSystemSchema(relation->schema);
      if (!(*iter)[0].IsEmpty()) {
	relation->oid = ReadOid(iter, 0);
	relation->name = ReadText(iter, 2);
	wxString relkind(ReadText(iter, 3));
	relation->extension = ReadText(iter, 4);
	relation->type = typemap[relkind];
      }
      databaseModel->relations.push_back(relation);
    }
  }
  void LoadFunctions() {
    QueryResults functionRows;
    std::map<wxString, FunctionModel::Type> typemap;
    typemap[_T("f")] = FunctionModel::SCALAR;
    typemap[_T("ft")] = FunctionModel::TRIGGER;
    typemap[_T("fs")] = FunctionModel::RECORDSET;
    typemap[_T("fa")] = FunctionModel::AGGREGATE;
    typemap[_T("fw")] = FunctionModel::WINDOW;
    DoQuery(_T("Functions"), functionRows);
    for (QueryResults::iterator iter = functionRows.begin(); iter != functionRows.end(); iter++) {
      FunctionModel *func = new FunctionModel();
      func->database = databaseModel;
      func->oid = ReadOid(iter, 0);
      func->schema = ReadText(iter, 1);
      func->name = ReadText(iter, 2);
      func->arguments = ReadText(iter, 3);
      wxString type(ReadText(iter, 4));
      func->extension = ReadText(iter, 5);
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

class LoadDatabaseDescriptionsWork : public ObjectBrowserWork {
public:
  LoadDatabaseDescriptionsWork(DatabaseModel *databaseModel) : databaseModel(databaseModel) {
    wxLogDebug(_T("%p: work to load schema object descriptions"), this);
  }
private:
  DatabaseModel *databaseModel;
  std::map<unsigned long, wxString> descriptions;
protected:
  void Execute() {
    QueryResults rs;
    DoQuery(_T("Object Descriptions"), rs);
    for (QueryResults::iterator iter = rs.begin(); iter != rs.end(); iter++) {
      unsigned long oid;
      wxString description;
      oid = ReadOid(iter, 0);
      description = ReadText(iter, 1);
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

class IndexSchemaCompletionCallback {
public:
  virtual void Completed(ObjectBrowser *ob, DatabaseModel *db, const CatalogueIndex *index) = 0;
};

class IndexDatabaseSchemaWork : public ObjectBrowserWork {
public:
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
    QueryResults rs;
    DoQuery(_T("IndexSchema"), rs);
    catalogueIndex = new CatalogueIndex();
    catalogueIndex->Begin();
    for (QueryResults::iterator iter = rs.begin(); iter != rs.end(); iter++) {
      long entityId;
      wxString typeString;
      wxString symbol;
      wxString disambig;
      entityId = ReadInt8(iter, 0);
      typeString = ReadText(iter, 1);
      symbol = ReadText(iter, 2);
      disambig = ReadText(iter, 3);
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

class LoadRelationWork : public ObjectBrowserWork {
public:
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
    QueryResults attributeRows;
    wxString oidValue;
    oidValue.Printf(_T("%d"), relationModel->oid);
    DoQuery(_T("Columns"), attributeRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = attributeRows.begin(); iter != attributeRows.end(); iter++) {
      ColumnModel *column = new ColumnModel();
      column->relation = relationModel;
      column->name = ReadText(iter, 0);
      column->type = ReadText(iter, 1);
      column->nullable = ReadBool(iter, 2);
      column->hasDefault = ReadBool(iter, 3);
      column->description = ReadText(iter, 4);
      columns.push_back(column);
    }
  }
  void ReadIndices() {
    wxString oidValue = wxString::Format(_T("%d"), relationModel->oid);
    QueryResults indexRows;
    DoQuery(_T("Indices"), indexRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = indexRows.begin(); iter != indexRows.end(); iter++) {
      IndexModel *index = new IndexModel();
      index->name = ReadText(iter, 0);
      indices.push_back(index);
    }
  }
  void ReadTriggers() {
    wxString oidValue = wxString::Format(_T("%d"), relationModel->oid);
    QueryResults triggerRows;
    DoQuery(_T("Triggers"), triggerRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = triggerRows.begin(); iter != triggerRows.end(); iter++) {
      TriggerModel *trigger = new TriggerModel();
      trigger->name = ReadText(iter, 0);
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

class ScriptWork : public ObjectBrowserWork {
public:
  enum Mode { Create, Alter, Drop, Select, Insert, Update, Delete };
  enum Output { Window, File, Clipboard };
  ScriptWork(DatabaseModel *database, Mode mode, Output output) : database(database), mode(mode), output(output) {}
protected:
  std::vector<wxString> statements;
  void LoadIntoView(ObjectBrowser *ob) {
    wxString message;
    switch (output) {
    case Window: {
      wxCommandEvent evt(PQWX_ScriptToWindow);
      wxString script;
      for (std::vector<wxString>::iterator iter = statements.begin(); iter != statements.end(); iter++) {
	script << *iter << _T("\n\n");
      }
      evt.SetString(script);
      evt.SetClientObject(database);
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

class DatabaseScriptWork : public ScriptWork {
public:
  DatabaseScriptWork(DatabaseModel *database, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(database, mode, output) {
    wxLogDebug(_T("%p: work to generate database script"), this);
  }
protected:
  void Execute() {
    QueryResults rs;
    DoQuery(_T("Database Detail"), rs, 26 /* oid */, database->oid);
    wxASSERT(rs.size() == 1);
    wxASSERT(rs[0].size() >= 5);
    wxString ownerName = rs[0][0];
    wxString encoding = rs[0][1];
    wxString collation = rs[0][2];
    wxString ctype = rs[0][3];
    long connectionLimit;
    rs[0][4].ToLong(&connectionLimit);

    wxString sql;
    switch (mode) {
    case Create:
      sql << _T("CREATE DATABASE ") << QuoteIdent(database->name);
      sql << _T("\n\tENCODING = ") << QuoteLiteral(encoding);
      sql << _T("\n\tLC_COLLATE = ") << QuoteLiteral(collation);
      sql << _T("\n\tLC_CTYPE = ") << QuoteLiteral(ctype);
      sql << _T("\n\tCONNECTION LIMIT = ") << connectionLimit;
      sql << _T("\n\tOWNER = ") << QuoteLiteral(ownerName);
      break;

    case Alter:
      sql << _T("ALTER DATABASE ") << QuoteIdent(database->name);
      sql << _T("\n\tOWNER = ") << QuoteLiteral(ownerName);
      sql << _T("\n\tCONNECTION LIMIT = ") << connectionLimit;
      break;

    case Drop:
      sql << _T("DROP DATABASE ") << QuoteIdent(database->name);
      break;

    default:
      wxASSERT(false);
    }

    statements.push_back(sql);
  }
};

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
