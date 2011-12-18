// -*- mode: c++ -*-

#ifndef __object_browser_database_work_h
#define __object_browser_database_work_h

typedef std::vector< std::vector<wxString> > QueryResults;

class ObjectBrowserWork : public DatabaseWork {
public:
  ObjectBrowserWork(ObjectBrowser *owner) : owner(owner) {}
  virtual ~ObjectBrowserWork() {}
  void execute(PGconn *conn) {
    if (PQserverVersion(conn) >= 80000) {
      if (!doCommand(conn, "BEGIN ISOLATION LEVEL SERIALIZABLE READ ONLY"))
	return;
    }
    else {
      if (!doCommand(conn, "BEGIN"))
	return;
      if (!doCommand(conn, "SET TRANSACTION ISOLATION LEVEL SERIALIZABLE"))
	return;
      if (!doCommand(conn, "SET TRANSACTION READ ONLY"))
	return;
    }

    executeInTransaction(conn);

    doCommand(conn, "END");
  }
  virtual void loadResultsToGui(ObjectBrowser *browser) = 0;
protected:
  void notifyFinished() {
    wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, EVENT_WORK_FINISHED);
    event.SetClientData(this);
    owner->AddPendingEvent(event);
  }
  virtual void executeInTransaction(PGconn *conn) = 0;
  bool doCommand(PGconn *conn, const char *sql) {
    return cmd(conn, sql);
  }
  bool doQuery(PGconn *conn, const wxString &name, QueryResults &results, Oid paramType, const char *paramValue) {
    doQuery(conn, GetSql(conn, name), results, paramType, paramValue);
  }
  bool doQuery(PGconn *conn, const wxString &name, QueryResults &results) {
    doQuery(conn, GetSql(conn, name), results);
  }
  bool doQuery(PGconn *conn, const char *sql, QueryResults &results, Oid paramType, const char *paramValue) {
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

    loadResultSet(rs, results);

    PQclear(rs);

    return true;
  }
  bool doQuery(PGconn *conn, const char *sql, QueryResults &results) {
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

    loadResultSet(rs, results);

    PQclear(rs);

    return true;
  }
  void loadResultSet(PGresult *rs, QueryResults &results) {
    int rowCount = PQntuples(rs);
    int colCount = PQnfields(rs);
    for (int rowNum = 0; rowNum < rowCount; rowNum++) {
      vector<wxString> row;
      for (int colNum = 0; colNum < colCount; colNum++) {
	const char *value = PQgetvalue(rs, rowNum, colNum);
	row.push_back(wxString(value, wxConvUTF8));
      }
      results.push_back(row);
    }
  }
private:
  ObjectBrowser *owner;
  const char *GetSql(PGconn *conn, const wxString &name) const {
    return owner->GetSql(name, PQserverVersion(conn));
  }
};

#define CHECK_INDEX(iter, index) wxASSERT(index < (*iter).size())
#define GET_OID(iter, index, result) CHECK_INDEX(iter, index); (*iter)[index].ToULong(&(result))
#define GET_BOOLEAN(iter, index, result) CHECK_INDEX(iter, index); result = (*iter)[index].IsSameAs(_T("t"))
#define GET_TEXT(iter, index, result) CHECK_INDEX(iter, index); result = (*iter)[index]
#define GET_INT4(iter, index, result) CHECK_INDEX(iter, index); (*iter)[index].ToLong(&(result))
#define GET_INT8(iter, index, result) CHECK_INDEX(iter, index); (*iter)[index].ToLong(&(result))

class RefreshDatabaseListWork : public ObjectBrowserWork {
public:
  RefreshDatabaseListWork(ObjectBrowser *owner, ServerModel *serverModel, wxTreeItemId serverItem) : ObjectBrowserWork(owner), serverModel(serverModel), serverItem(serverItem) {
    wxLogDebug(_T("%p: work to load database list"), this);
  }
protected:
  void executeInTransaction(PGconn *conn) {
    loadServer(conn);
    loadDatabases(conn);
    loadRoles(conn);
  }
  void loadResultsToGui(ObjectBrowser *ob) {
    ob->FillInServer(serverModel, serverItem, serverVersionString, serverVersion, usingSSL);
    ob->FillInDatabases(serverModel, serverItem, databases);
    ob->FillInRoles(serverModel, serverItem, roles);

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
  void loadServer(PGconn *conn) {
    const char *serverVersionRaw = PQparameterStatus(conn, "server_version");
    serverVersionString = wxString(serverVersionRaw, wxConvUTF8);
    serverVersion = PQserverVersion(conn);
    void *ssl = PQgetssl(conn);
    usingSSL = (ssl != NULL);
  }
  void loadDatabases(PGconn *conn) {
    QueryResults databaseRows;
    doQuery(conn, _T("Databases"), databaseRows);
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
  void loadRoles(PGconn *conn) {
    QueryResults roleRows;
    doQuery(conn, _T("Roles"), roleRows);
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
  LoadDatabaseSchemaWork(ObjectBrowser *owner, DatabaseModel *databaseModel, wxTreeItemId databaseItem) : ObjectBrowserWork(owner), databaseModel(databaseModel), databaseItem(databaseItem) {
    wxLogDebug(_T("%p: work to load schema"), this);
  }
private:
  DatabaseModel *databaseModel;
  wxTreeItemId databaseItem;
  vector<RelationModel*> relations;
  vector<FunctionModel*> functions;
protected:
  void executeInTransaction(PGconn *conn) {
    loadRelations(conn);
    loadFunctions(conn);
  }
  void loadRelations(PGconn *conn) {
    QueryResults relationRows;
    map<wxString, RelationModel::Type> typemap;
    typemap[_T("r")] = RelationModel::TABLE;
    typemap[_T("v")] = RelationModel::VIEW;
    typemap[_T("S")] = RelationModel::SEQUENCE;
    doQuery(conn, _T("Relations"), relationRows);
    for (QueryResults::iterator iter = relationRows.begin(); iter != relationRows.end(); iter++) {
      RelationModel *relation = new RelationModel();
      relation->database = databaseModel;
      GET_OID(iter, 0, relation->oid);
      GET_TEXT(iter, 1, relation->schema);
      GET_TEXT(iter, 2, relation->name);
      wxString relkind;
      GET_TEXT(iter, 3, relkind);
      relation->type = typemap[relkind];
      relation->user = !systemSchema(relation->schema);
      GET_TEXT(iter, 4, relation->description);
      relations.push_back(relation);
    }
  }
  void loadFunctions(PGconn *conn) {
    QueryResults functionRows;
    map<wxString, FunctionModel::Type> typemap;
    typemap[_T("f")] = FunctionModel::SCALAR;
    typemap[_T("fs")] = FunctionModel::RECORDSET;
    typemap[_T("fa")] = FunctionModel::AGGREGATE;
    typemap[_T("fw")] = FunctionModel::WINDOW;
    doQuery(conn, _T("Functions"), functionRows);
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
      func->user = !systemSchema(func->schema);
      functions.push_back(func);
    }
  }
  void loadResultsToGui(ObjectBrowser *ob) {
    ob->FillInDatabaseSchema(databaseModel, databaseItem, relations, functions);
    ob->Expand(databaseItem);
    ob->SetItemText(databaseItem, databaseModel->name); // remove loading message
  }
private:
  static inline bool systemSchema(wxString schema) {
    return schema.StartsWith(_T("pg_")) || schema.IsSameAs(_T("information_schema"));
  }
};

class IndexDatabaseSchemaWork : public ObjectBrowserWork {
public:
  IndexDatabaseSchemaWork(ObjectBrowser *owner, DatabaseModel *database) : ObjectBrowserWork(owner), database(database) {
    wxLogDebug(_T("%p: work to index schema"), this);
  }
private:
  DatabaseModel *database;
  CatalogueIndex *catalogueIndex;
protected:
  void executeInTransaction(PGconn *conn) {
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
    doQuery(conn, _T("IndexSchema"), rs);
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
  void loadResultsToGui(ObjectBrowser *ob) {
    database->catalogueIndex = catalogueIndex;
  }
};

class LoadRelationWork : public ObjectBrowserWork {
public:
  LoadRelationWork(ObjectBrowser *owner, RelationModel *relationModel, wxTreeItemId relationItem) : ObjectBrowserWork(owner), relationModel(relationModel), relationItem(relationItem) {
    wxLogDebug(_T("%p: work to load relation"), this);
  }
private:
  RelationModel *relationModel;
  wxTreeItemId relationItem;
  vector<ColumnModel*> columns;
  vector<IndexModel*> indices;
  vector<TriggerModel*> triggers;
protected:
  void executeInTransaction(PGconn *conn) {
    loadColumns(conn);
    loadIndices(conn);
    loadTriggers(conn);
  }
private:
  void loadColumns(PGconn *conn) {
    QueryResults attributeRows;
    wxString oidValue;
    oidValue.Printf(_T("%d"), relationModel->oid);
    doQuery(conn, _T("Columns"), attributeRows, 26 /* oid */, oidValue.utf8_str());
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
  void loadIndices(PGconn *conn) {
    wxString oidValue = wxString::Format(_T("%d"), relationModel->oid);
    QueryResults indexRows;
    doQuery(conn, _T("Indices"), indexRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = indexRows.begin(); iter != indexRows.end(); iter++) {
      IndexModel *index = new IndexModel();
      GET_TEXT(iter, 0, index->name);
      indices.push_back(index);
    }
  }
  void loadTriggers(PGconn *conn) {
    wxString oidValue = wxString::Format(_T("%d"), relationModel->oid);
    QueryResults triggerRows;
    doQuery(conn, _T("Triggers"), triggerRows, 26 /* oid */, oidValue.utf8_str());
    for (QueryResults::iterator iter = triggerRows.begin(); iter != triggerRows.end(); iter++) {
      TriggerModel *trigger = new TriggerModel();
      GET_TEXT(iter, 0, trigger->name);
      triggers.push_back(trigger);
    }
  }
protected:
  void loadResultsToGui(ObjectBrowser *ob) {
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

#endif
