// -*- mode: c++ -*-

#ifndef __object_browser_database_work_h
#define __object_browser_database_work_h

typedef std::vector< std::vector<wxString> > QueryResults;

class ObjectBrowserWork {
public:
  virtual void Execute(PGconn *conn) = 0;
  virtual void LoadIntoView(ObjectBrowser *browser) = 0;
protected:
  bool DoQuery(PGconn *conn, const wxString &name, QueryResults &results, Oid paramType, unsigned long paramValue) {
    wxString valueString = wxString::Format(_T("%lu"), paramValue);
    DoQuery(conn, GetSql(conn, name), results, paramType, valueString.utf8_str());
  }
  bool DoQuery(PGconn *conn, const wxString &name, QueryResults &results, Oid paramType, const char *paramValue) {
    DoQuery(conn, GetSql(conn, name), results, paramType, paramValue);
  }
  bool DoQuery(PGconn *conn, const wxString &name, QueryResults &results) {
    DoQuery(conn, GetSql(conn, name), results);
  }
  bool DoQuery(PGconn *conn, const char *sql, QueryResults &results, Oid paramType, const char *paramValue) {
    logger->LogSql(sql);

    PGresult *rs = PQexecParams(conn, sql, 1, &paramType, &paramValue, NULL, NULL, 0);
    if (!rs)
      return false;

    ExecStatusType status = PQresultStatus(rs);
    if (status != PGRES_TUPLES_OK) {
#ifdef PQWX_DEBUG
      logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
      return false; // expected data back
    }

    ReadResultSet(rs, results);

    PQclear(rs);

    return true;
  }
  bool DoQuery(PGconn *conn, const char *sql, QueryResults &results) {
    logger->LogSql(sql);

    PGresult *rs = PQexec(conn, sql);
    if (!rs)
      return false;

    ExecStatusType status = PQresultStatus(rs);
    if (status != PGRES_TUPLES_OK) {
#ifdef PQWX_DEBUG
      logger->LogSqlQueryFailed(PQresultErrorMessage(rs), status);
#endif
      return false; // expected data back
    }

    ReadResultSet(rs, results);

    PQclear(rs);

    return true;
  }
  void ReadResultSet(PGresult *rs, QueryResults &results) {
    int rowCount = PQntuples(rs);
    int colCount = PQnfields(rs);
    results.reserve(rowCount);
    for (int rowNum = 0; rowNum < rowCount; rowNum++) {
      vector<wxString> row;
      row.reserve(colCount);
      for (int colNum = 0; colNum < colCount; colNum++) {
	const char *value = PQgetvalue(rs, rowNum, colNum);
	row.push_back(wxString(value, wxConvUTF8));
      }
      results.push_back(row);
    }
  }
  const char *GetSql(PGconn *conn, const wxString &name) const {
    return ObjectBrowser::GetSqlDictionary().GetSql(name, PQserverVersion(conn));
  }
  SqlLogger *logger;
  friend class ObjectBrowserDatabaseWork;
};

class ObjectBrowserDatabaseWork : public DatabaseWork {
public:
  ObjectBrowserDatabaseWork(wxEvtHandler *dest, ObjectBrowserWork *work) : dest(dest), work(work) {}
  void Execute(PGconn *conn) {
    work->logger = logger;
    work->Execute(conn);
  }
  void NotifyFinished() {
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, EVENT_WORK_FINISHED);
    event.SetClientData(work);
    dest->AddPendingEvent(event);
  }
private:
  wxEvtHandler *dest;
  ObjectBrowserWork *work;
};

#define CHECK_INDEX(iter, index) wxASSERT(index < (*iter).size())
#define GET_OID(iter, index, result) CHECK_INDEX(iter, index); (*iter)[index].ToULong(&(result))
#define GET_BOOLEAN(iter, index, result) CHECK_INDEX(iter, index); result = (*iter)[index].IsSameAs(_T("t"))
#define GET_TEXT(iter, index, result) CHECK_INDEX(iter, index); result = (*iter)[index]
#define GET_INT4(iter, index, result) CHECK_INDEX(iter, index); (*iter)[index].ToLong(&(result))
#define GET_INT8(iter, index, result) CHECK_INDEX(iter, index); (*iter)[index].ToLong(&(result))

class RefreshDatabaseListWork : public ObjectBrowserWork {
public:
  RefreshDatabaseListWork(ServerModel *serverModel, wxTreeItemId serverItem) : serverModel(serverModel), serverItem(serverItem) {
    wxLogDebug(_T("%p: work to load database list"), this);
  }
protected:
  void Execute(PGconn *conn) {
    ReadServer(conn);
    ReadDatabases(conn);
    ReadRoles(conn);
  }
  void LoadIntoView(ObjectBrowser *ob) {
    ob->FillInServer(serverModel, serverItem, serverVersionString, serverVersion, usingSSL);
    ob->FillInDatabases(serverModel, serverItem, databases);
    ob->FillInRoles(serverModel, serverItem, roles);

    ob->EnsureVisible(serverItem);
    ob->SelectItem(serverItem);
    ob->Expand(serverItem);
  }
private:
  ServerModel *serverModel;
  wxTreeItemId serverItem;
  vector<DatabaseModel*> databases;
  vector<RoleModel*> roles;
  wxString serverVersionString;
  int serverVersion;
  bool usingSSL;
  void ReadServer(PGconn *conn) {
    const char *serverVersionRaw = PQparameterStatus(conn, "server_version");
    serverVersionString = wxString(serverVersionRaw, wxConvUTF8);
    serverVersion = PQserverVersion(conn);
    void *ssl = PQgetssl(conn);
    usingSSL = (ssl != NULL);
  }
  void ReadDatabases(PGconn *conn) {
    QueryResults databaseRows;
    DoQuery(conn, _T("Databases"), databaseRows);
    for (QueryResults::iterator iter = databaseRows.begin(); iter != databaseRows.end(); iter++) {
      DatabaseModel *database = new DatabaseModel();
      database->server = serverModel;
      database->loaded = false;
      GET_OID(iter, 0, database->oid);
      GET_TEXT(iter, 1, database->name);
      GET_BOOLEAN(iter, 2, database->isTemplate);
      GET_BOOLEAN(iter, 3, database->allowConnections);
      GET_BOOLEAN(iter, 4, database->havePrivsToConnect);
      GET_TEXT(iter, 5, database->description);
      databases.push_back(database);
    }
  }
  void ReadRoles(PGconn *conn) {
    QueryResults roleRows;
    DoQuery(conn, _T("Roles"), roleRows);
    for (QueryResults::iterator iter = roleRows.begin(); iter != roleRows.end(); iter++) {
      RoleModel *role = new RoleModel();
      GET_OID(iter, 0, role->oid);
      GET_TEXT(iter, 1, role->name);
      GET_BOOLEAN(iter, 2, role->canLogin);
      GET_BOOLEAN(iter, 3, role->superuser);
      GET_TEXT(iter, 4, role->description);
      roles.push_back(role);
    }
  }
};

class LoadDatabaseSchemaWork : public ObjectBrowserWork {
public:
  LoadDatabaseSchemaWork(DatabaseModel *databaseModel, wxTreeItemId databaseItem) : databaseModel(databaseModel), databaseItem(databaseItem) {
    wxLogDebug(_T("%p: work to load schema"), this);
  }
private:
  DatabaseModel *databaseModel;
  wxTreeItemId databaseItem;
  vector<RelationModel*> relations;
  vector<FunctionModel*> functions;
protected:
  void Execute(PGconn *conn) {
    LoadRelations(conn);
    LoadFunctions(conn);
  }
  void LoadRelations(PGconn *conn) {
    QueryResults relationRows;
    map<wxString, RelationModel::Type> typemap;
    typemap[_T("r")] = RelationModel::TABLE;
    typemap[_T("v")] = RelationModel::VIEW;
    typemap[_T("S")] = RelationModel::SEQUENCE;
    DoQuery(conn, _T("Relations"), relationRows);
    for (QueryResults::iterator iter = relationRows.begin(); iter != relationRows.end(); iter++) {
      RelationModel *relation = new RelationModel();
      relation->database = databaseModel;
      GET_OID(iter, 0, relation->oid);
      GET_TEXT(iter, 1, relation->schema);
      GET_TEXT(iter, 2, relation->name);
      wxString relkind;
      GET_TEXT(iter, 3, relkind);
      relation->type = typemap[relkind];
      relation->user = !IsSystemSchema(relation->schema);
      GET_TEXT(iter, 4, relation->description);
      relations.push_back(relation);
    }
  }
  void LoadFunctions(PGconn *conn) {
    QueryResults functionRows;
    map<wxString, FunctionModel::Type> typemap;
    typemap[_T("f")] = FunctionModel::SCALAR;
    typemap[_T("fs")] = FunctionModel::RECORDSET;
    typemap[_T("fa")] = FunctionModel::AGGREGATE;
    typemap[_T("fw")] = FunctionModel::WINDOW;
    DoQuery(conn, _T("Functions"), functionRows);
    for (QueryResults::iterator iter = functionRows.begin(); iter != functionRows.end(); iter++) {
      FunctionModel *func = new FunctionModel();
      func->database = databaseModel;
      GET_OID(iter, 0, func->oid);
      GET_TEXT(iter, 1, func->schema);
      GET_TEXT(iter, 2, func->name);
      GET_TEXT(iter, 3, func->prototype);
      wxString type;
      GET_TEXT(iter, 4, type);
      func->type = typemap[type];
      GET_TEXT(iter, 5, func->description);
      func->user = !IsSystemSchema(func->schema);
      functions.push_back(func);
    }
  }
  void LoadIntoView(ObjectBrowser *ob) {
    ob->FillInDatabaseSchema(databaseModel, databaseItem, relations, functions);
    ob->Expand(databaseItem);
    ob->SetItemText(databaseItem, databaseModel->name); // remove loading message
  }
private:
  static inline bool IsSystemSchema(wxString schema) {
    return schema.StartsWith(_T("pg_")) || schema.IsSameAs(_T("information_schema"));
  }
};

class IndexDatabaseSchemaWork : public ObjectBrowserWork {
public:
  IndexDatabaseSchemaWork(DatabaseModel *database) : database(database) {
    wxLogDebug(_T("%p: work to index schema"), this);
  }
private:
  DatabaseModel *database;
  CatalogueIndex *catalogueIndex;
protected:
  void Execute(PGconn *conn) {
    map<wxString, CatalogueIndex::Type> typeMap;
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
    DoQuery(conn, _T("IndexSchema"), rs);
    catalogueIndex = new CatalogueIndex();
    catalogueIndex->Begin();
    for (QueryResults::iterator iter = rs.begin(); iter != rs.end(); iter++) {
      long entityId;
      wxString typeString;
      wxString symbol;
      wxString disambig;
      GET_INT8(iter, 0, entityId);
      GET_TEXT(iter, 1, typeString);
      GET_TEXT(iter, 2, symbol);
      GET_TEXT(iter, 3, disambig);
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
  vector<ColumnModel*> columns;
  vector<IndexModel*> indices;
  vector<TriggerModel*> triggers;
protected:
  void Execute(PGconn *conn) {
    ReadColumns(conn);
    ReadIndices(conn);
    ReadTriggers(conn);
  }
private:
  void ReadColumns(PGconn *conn) {
    QueryResults attributeRows;
    wxString oidValue;
    oidValue.Printf(_T("%d"), relationModel->oid);
    DoQuery(conn, _T("Columns"), attributeRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = attributeRows.begin(); iter != attributeRows.end(); iter++) {
      ColumnModel *column = new ColumnModel();
      column->relation = relationModel;
      GET_TEXT(iter, 0, column->name);
      GET_TEXT(iter, 1, column->type);
      GET_BOOLEAN(iter, 2, column->nullable);
      GET_BOOLEAN(iter, 3, column->hasDefault);
      GET_TEXT(iter, 4, column->description);
      columns.push_back(column);
    }
  }
  void ReadIndices(PGconn *conn) {
    wxString oidValue = wxString::Format(_T("%d"), relationModel->oid);
    QueryResults indexRows;
    DoQuery(conn, _T("Indices"), indexRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = indexRows.begin(); iter != indexRows.end(); iter++) {
      IndexModel *index = new IndexModel();
      GET_TEXT(iter, 0, index->name);
      indices.push_back(index);
    }
  }
  void ReadTriggers(PGconn *conn) {
    wxString oidValue = wxString::Format(_T("%d"), relationModel->oid);
    QueryResults triggerRows;
    DoQuery(conn, _T("Triggers"), triggerRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = triggerRows.begin(); iter != triggerRows.end(); iter++) {
      TriggerModel *trigger = new TriggerModel();
      GET_TEXT(iter, 0, trigger->name);
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
  const enum Mode { Create, Alter, Drop } mode;
  const enum Output { Window, File, Clipboard } output;
  ScriptWork(Mode mode, Output output): mode(mode), output(output) {}
protected:
  std::vector<wxString> ddl;
  void LoadIntoView(ObjectBrowser *ob) {
    wxString message;
    switch (output) {
    case Window:
      message << _T("TODO Send to query window:\n\n");
      break;
    case File:
      message << _T("TODO Send to file:\n\n");
      break;
    case Clipboard:
      message << _T("TODO Send to clipboard:\n\n");
      break;
    }
    for (std::vector<wxString>::iterator iter = ddl.begin(); iter != ddl.end(); iter++) {
      message << *iter << _T("\n\n");
    }
    wxLogDebug(message);
    wxMessageBox(message);
  }
};

class DatabaseScriptWork : public ScriptWork {
public:
  DatabaseScriptWork(DatabaseModel *database, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(mode, output), database(database) {
    wxLogDebug(_T("%p: work to generate database DDL script"), this);
  }
private:
  DatabaseModel *database;
protected:
  void Execute(PGconn *conn) {
    QueryResults rs;
    DoQuery(conn, _T("Database Detail"), rs, 26 /* oid */, database->oid);
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
      sql << _T("CREATE DATABASE ") << DatabaseWork::QuoteIdent(conn, database->name);
      sql << _T("\n\tENCODING = ") << DatabaseWork::QuoteLiteral(conn, encoding);
      sql << _T("\n\tLC_COLLATE = ") << DatabaseWork::QuoteLiteral(conn, collation);
      sql << _T("\n\tLC_CTYPE = ") << DatabaseWork::QuoteLiteral(conn, ctype);
      sql << _T("\n\tCONNECTION LIMIT = ") << connectionLimit;
      sql << _T("\n\tOWNER = ") << DatabaseWork::QuoteLiteral(conn, ownerName);
      break;

    case Alter:
      sql << _T("ALTER DATABASE ") << DatabaseWork::QuoteIdent(conn, database->name);
      sql << _T("\n\tOWNER = ") << DatabaseWork::QuoteLiteral(conn, ownerName);
      sql << _T("\n\tCONNECTION LIMIT = ") << connectionLimit;
      break;

    case Drop:
      sql << _T("DROP DATABASE ") << DatabaseWork::QuoteIdent(conn, database->name);
      break;
    }

    ddl.push_back(sql);
  }
};

class TableScriptWork : public ScriptWork {
public:
  TableScriptWork(RelationModel *table, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(mode, output), table(table) {
    wxLogDebug(_T("%p: work to generate table DDL script"), this);
  }
private:
  RelationModel *table;
protected:
  void Execute(PGconn *conn) {
  }
};

class ViewScriptWork : public ScriptWork {
public:
  ViewScriptWork(RelationModel *view, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(mode, output), view(view) {
    wxLogDebug(_T("%p: work to generate view DDL script"), this);
  }
private:
  RelationModel *view;
protected:
  void Execute(PGconn *conn) {
  }
};

class SequenceScriptWork : public ScriptWork {
public:
  SequenceScriptWork(RelationModel *sequence, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(mode, output), sequence(sequence) {
    wxLogDebug(_T("%p: work to generate sequence DDL script"), this);
  }
private:
  RelationModel *sequence;
protected:
  void Execute(PGconn *conn) {
  }
};

class FunctionScriptWork : public ScriptWork {
public:
  FunctionScriptWork(FunctionModel *function, ScriptWork::Mode mode, ScriptWork::Output output) : ScriptWork(mode, output), function(function) {
    wxLogDebug(_T("%p: work to generate function DDL script"), this);
  }
private:
  FunctionModel *function;
protected:
  void Execute(PGconn *conn) {
  }
};

#endif
