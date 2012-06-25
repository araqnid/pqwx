#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_database_work.h"

/*
 * Database list, and other server globals.
 */
 
void RefreshDatabaseListWork::operator()()
{
  ReadServer();
  ReadDatabases();
  ReadRoles();
  ReadTablespaces();
}

void RefreshDatabaseListWork::ReadServer()
{
  serverVersionString = wxString(PQparameterStatus(conn, "server_version"), wxConvUTF8);
  serverVersion = PQserverVersion(conn);
  ssl = (SSL*) PQgetssl(conn);
}

void RefreshDatabaseListWork::ReadDatabases()
{
  QueryResults databaseRows = Query(_T("Databases")).List();
  for (QueryResults::const_iterator iter = databaseRows.begin(); iter != databaseRows.end(); iter++) {
    DatabaseModel database;
    database.oid = (*iter).ReadOid(0);
    database.name = (*iter).ReadText(1);
    database.isTemplate = (*iter).ReadBool(2);
    database.allowConnections = (*iter).ReadBool(3);
    database.havePrivsToConnect = (*iter).ReadBool(4);
    database.description = (*iter).ReadText(5);
    databases.push_back(database);
  }
  sort(databases.begin(), databases.end(), ObjectModel::CollateByName);
}

void RefreshDatabaseListWork::ReadRoles()
{
  QueryResults roleRows = Query(_T("Roles")).List();
  for (QueryResults::const_iterator iter = roleRows.begin(); iter != roleRows.end(); iter++) {
    RoleModel role;
    role.oid = (*iter).ReadOid(0);
    role.name = (*iter).ReadText(1);
    role.canLogin = (*iter).ReadBool(2);
    role.superuser = (*iter).ReadBool(3);
    role.description = (*iter).ReadText(4);
    roles.push_back(role);
  }
  sort(roles.begin(), roles.end(), ObjectModel::CollateByName);
}

void RefreshDatabaseListWork::ReadTablespaces()
{
  QueryResults rows = Query(_T("Tablespaces")).List();
  for (QueryResults::const_iterator iter = rows.begin(); iter != rows.end(); iter++) {
    TablespaceModel spc;
    spc.oid = (*iter).ReadOid(0);
    spc.name = (*iter).ReadText(1);
    spc.location = (*iter).ReadText(2);
    tablespaces.push_back(spc);
  }
}

void RefreshDatabaseListWork::UpdateModel(ObjectBrowserModel *model)
{
  ServerModel *server = model->FindServer(serverId);
  server->UpdateServerParameters(serverVersionString, serverVersion, ssl);
  server->UpdateDatabases(databases);
  server->UpdateRoles(roles);
  server->UpdateTablespaces(tablespaces);
}

void RefreshDatabaseListWork::UpdateView(ObjectBrowser *ob)
{
  ob->UpdateServer(serverId, true);
}

/*
 * Schema of a single database
 */

static std::map<wxString, RelationModel::Type> InitRelationTypeMap()
{
  std::map<wxString, RelationModel::Type> typemap;
  typemap[_T("r")] = RelationModel::TABLE;
  typemap[_T("v")] = RelationModel::VIEW;
  typemap[_T("S")] = RelationModel::SEQUENCE;
  return typemap;
}

const std::map<wxString, RelationModel::Type> LoadDatabaseSchemaWork::relationTypeMap = InitRelationTypeMap();

static std::map<wxString, FunctionModel::Type> InitFunctionTypeMap()
{
  std::map<wxString, FunctionModel::Type> typemap;
  typemap[_T("f")] = FunctionModel::SCALAR;
  typemap[_T("ft")] = FunctionModel::TRIGGER;
  typemap[_T("fs")] = FunctionModel::RECORDSET;
  typemap[_T("fa")] = FunctionModel::AGGREGATE;
  typemap[_T("fw")] = FunctionModel::WINDOW;
  return typemap;
}

const std::map<wxString, FunctionModel::Type> LoadDatabaseSchemaWork::functionTypeMap = InitFunctionTypeMap();

void LoadDatabaseSchemaWork::operator()() {
  incoming.oid = databaseRef.GetOid();
  LoadRelations();
  LoadFunctions();
  LoadTextSearchParsers();
  LoadTextSearchDictionaries();
  LoadTextSearchTemplates();
  LoadTextSearchConfigurations();
}

void LoadDatabaseSchemaWork::LoadRelations() {
  QueryResults relationRows = Query(_T("Relations")).List();
  for (QueryResults::const_iterator iter = relationRows.begin(); iter != relationRows.end(); iter++) {
    RelationModel relation;
    relation.schema = (*iter).ReadText(1);
    if (!(*iter)[0].IsEmpty()) {
      relation.oid = (*iter).ReadOid(0);
      relation.name = (*iter).ReadText(2);
      wxString relkind((*iter).ReadText(3));
      relation.extension = (*iter).ReadText(4);
      relation.unlogged = (*iter).ReadBool(5);
      wxASSERT_MSG(relationTypeMap.count(relkind) > 0, relkind);
      relation.type = relationTypeMap.find(relkind)->second;
    }
    incoming.relations.push_back(relation);
  }
}

void LoadDatabaseSchemaWork::LoadFunctions() {
  QueryResults functionRows = Query(_T("Functions")).List();
  for (QueryResults::const_iterator iter = functionRows.begin(); iter != functionRows.end(); iter++) {
    FunctionModel func;
    func.oid = (*iter).ReadOid(0);
    func.schema = (*iter).ReadText(1);
    func.name = (*iter).ReadText(2);
    func.arguments = (*iter).ReadText(3);
    wxString type((*iter).ReadText(4));
    func.extension = (*iter).ReadText(5);
    wxASSERT_MSG(functionTypeMap.count(type) > 0, type);
    func.type = functionTypeMap.find(type)->second;
    incoming.functions.push_back(func);
  }
}

void LoadDatabaseSchemaWork::LoadTextSearchDictionaries()
{
  QueryResults rows = Query(_T("Text search dictionaries")).List();
  for (QueryResults::const_iterator iter = rows.begin(); iter != rows.end(); iter++) {
    TextSearchDictionaryModel dict;
    dict.oid = (*iter).ReadOid(0);
    dict.schema = (*iter).ReadText(1);
    dict.name = (*iter).ReadText(2);
    incoming.textSearchDictionaries.push_back(dict);
  }
}

void LoadDatabaseSchemaWork::LoadTextSearchParsers()
{
  QueryResults rows = Query(_T("Text search parsers")).List();
  for (QueryResults::const_iterator iter = rows.begin(); iter != rows.end(); iter++) {
    TextSearchParserModel prs;
    prs.oid = (*iter).ReadOid(0);
    prs.schema = (*iter).ReadText(1);
    prs.name = (*iter).ReadText(2);
    incoming.textSearchParsers.push_back(prs);
  }
}

void LoadDatabaseSchemaWork::LoadTextSearchTemplates()
{
  QueryResults rows = Query(_T("Text search templates")).List();
  for (QueryResults::const_iterator iter = rows.begin(); iter != rows.end(); iter++) {
    TextSearchTemplateModel tmpl;
    tmpl.oid = (*iter).ReadOid(0);
    tmpl.schema = (*iter).ReadText(1);
    tmpl.name = (*iter).ReadText(2);
    incoming.textSearchTemplates.push_back(tmpl);
  }
}

void LoadDatabaseSchemaWork::LoadTextSearchConfigurations()
{
  QueryResults rows = Query(_T("Text search configurations")).List();
  for (QueryResults::const_iterator iter = rows.begin(); iter != rows.end(); iter++) {
    TextSearchConfigurationModel cfg;
    cfg.oid = (*iter).ReadOid(0);
    cfg.schema = (*iter).ReadText(1);
    cfg.name = (*iter).ReadText(2);
    incoming.textSearchConfigurations.push_back(cfg);
  }
}

void LoadDatabaseSchemaWork::UpdateModel(ObjectBrowserModel *model)
{
  ServerModel *server = model->FindServer(databaseRef);
  wxASSERT(server != NULL);
  server->UpdateDatabase(incoming);
}

void LoadDatabaseSchemaWork::UpdateView(ObjectBrowser *ob)
{
  ob->UpdateDatabase(databaseRef, expandAfter);
}

/*
 * All database object descriptions.
 */
void LoadDatabaseDescriptionsWork::operator()()
{
  QueryResults rs = Query(_T("Object Descriptions")).List();
  for (QueryResults::const_iterator iter = rs.begin(); iter != rs.end(); iter++) {
    unsigned long oid;
    wxString description;
    oid = (*iter).ReadOid(0);
    description = (*iter).ReadText(1);
    descriptions[oid] = description;
  }
}

void LoadDatabaseDescriptionsWork::UpdateModel(ObjectBrowserModel *model)
{
}

void LoadDatabaseDescriptionsWork::UpdateView(ObjectBrowser *ob)
{
  int count = 0;
  for (std::vector<RelationModel>::iterator iter = databaseModel->relations.begin(); iter != databaseModel->relations.end(); iter++) {
    std::map<unsigned long, wxString>::const_iterator ptr = descriptions.find((*iter).oid);
    if (ptr != descriptions.end()) {
      (*iter).description = (*ptr).second;
      ++count;
    }
  }
  for (std::vector<FunctionModel>::iterator iter = databaseModel->functions.begin(); iter != databaseModel->functions.end(); iter++) {
    std::map<unsigned long, wxString>::const_iterator ptr = descriptions.find((*iter).oid);
    if (ptr != descriptions.end()) {
      (*iter).description = (*ptr).second;
      ++count;
    }
  }
  wxLogDebug(_T("Loaded %d/%lu descriptions"), count, descriptions.size());
}

/**
 * Compile database catalogue index.
 */
static std::map<wxString, CatalogueIndex::Type> InitIndexerTypeMap()
{
  std::map<wxString, CatalogueIndex::Type> typeMap;
  typeMap[_T("t")] = CatalogueIndex::TABLE;
  typeMap[_T("tu")] = CatalogueIndex::TABLE_UNLOGGED;
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
  typeMap[_T("F")] = CatalogueIndex::TEXT_CONFIGURATION;
  typeMap[_T("Fd")] = CatalogueIndex::TEXT_DICTIONARY;
  typeMap[_T("Fp")] = CatalogueIndex::TEXT_PARSER;
  typeMap[_T("Ft")] = CatalogueIndex::TEXT_TEMPLATE;
  return typeMap;
}

const std::map<wxString, CatalogueIndex::Type> IndexDatabaseSchemaWork::typeMap = InitIndexerTypeMap();

void IndexDatabaseSchemaWork::operator()() {
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

void IndexDatabaseSchemaWork::UpdateModel(ObjectBrowserModel *model)
{
  database->catalogueIndex = catalogueIndex;
}

void IndexDatabaseSchemaWork::UpdateView(ObjectBrowser *ob)
{
  if (completion) {
    completion->Completed(ob, database, catalogueIndex);
    delete completion;
  }
}

/*
 * Load a relation's details.
 */
void LoadRelationWork::operator()() {
  detail = new RelationModel();
  ReadColumns();
  if (relationType == RelationModel::TABLE) {
    ReadIndices();
    ReadTriggers();
    ReadSequences();
    ReadConstraints();
  }
}

void LoadRelationWork::ReadColumns() {
  QueryResults attributeRows = Query(_T("Columns")).OidParam(relationRef.GetOid()).List();
  for (QueryResults::const_iterator iter = attributeRows.begin(); iter != attributeRows.end(); iter++) {
    ColumnModel *column = new ColumnModel();
    column->name = (*iter).ReadText(0);
    column->type = (*iter).ReadText(1);
    column->nullable = (*iter).ReadBool(2);
    column->hasDefault = (*iter).ReadBool(3);
    column->description = (*iter).ReadText(4);
    column->attnum = (*iter).ReadInt4(5);
    detail->columns.push_back(column);
  }
}

void LoadRelationWork::ReadIndices() {
  QueryResults indexRows = Query(_T("Indices")).OidParam(relationRef.GetOid()).List();
  IndexModel *lastIndex = NULL;
  std::vector<int> indexColumns;
  std::vector<int>::const_iterator indexColumnsIter;
  for (QueryResults::const_iterator iter = indexRows.begin(); iter != indexRows.end(); iter++) {
    IndexModel *index;
    wxString indexName = (*iter)[_T("relname")];
    if (lastIndex == NULL || lastIndex->name != indexName) {
      index = new IndexModel();
      index->name = indexName;
      index->primaryKey = (*iter).ReadBool(_T("indisprimary"));
      index->unique = (*iter).ReadBool(_T("indisunique"));
      index->exclusion = (*iter).ReadBool(_T("indisexclusion"));
      index->clustered = (*iter).ReadBool(_T("indisclustered"));
      indexColumns = ParseInt2Vector((*iter)[_T("indkey")]);
      indexColumnsIter = indexColumns.begin();
      lastIndex = index;
      detail->indices.push_back(index);
    }
    else {
      index = lastIndex;
    }
    wxString expression = (*iter)[_T("indexattdef")];
    index->columns.push_back(IndexModel::Column(*indexColumnsIter++, expression));
  }
}

std::vector<int> LoadRelationWork::ParseInt2Vector(const wxString &str)
{
  std::vector<int> result;
  unsigned mark = 0;
  for (unsigned pos = 0; pos < str.length(); pos++) {
    if (str[pos] == ' ') {
      long value;
      if (str.Mid(mark, pos-mark).ToLong(&value)) {
        result.push_back((int) value);
      }
      mark = pos + 1;
    }
  }

  if (mark < str.length()) {
    long value;
    if (str.Mid(mark, str.length()-mark).ToLong(&value)) {
      result.push_back((int) value);
    }
  }

  return result;
}

void LoadRelationWork::ReadTriggers() {
  QueryResults triggerRows = Query(_T("Triggers")).OidParam(relationRef.GetOid()).List();
  for (QueryResults::const_iterator iter = triggerRows.begin(); iter != triggerRows.end(); iter++) {
    TriggerModel *trigger = new TriggerModel();
    trigger->name = (*iter).ReadText(0);
    detail->triggers.push_back(trigger);
  }
}

void LoadRelationWork::ReadSequences() {
  QueryResults sequenceRows = Query(_T("Sequences")).OidParam(relationRef.GetOid()).List();
  for (QueryResults::const_iterator iter = sequenceRows.begin(); iter != sequenceRows.end(); iter++) {
    RelationModel *sequence = new RelationModel();
    sequence->type = RelationModel::SEQUENCE;
    sequence->oid = (*iter).ReadOid(0);
    sequence->schema = (*iter).ReadText(1);
    sequence->name = (*iter).ReadText(2);
    sequence->owningColumn = (*iter).ReadInt4(3);
    detail->sequences.push_back(sequence);
  }
}

void LoadRelationWork::ReadConstraints() {
  QueryResults rows = Query(_T("Constraints")).OidParam(relationRef.GetOid()).List();
  for (QueryResults::const_iterator iter = rows.begin(); iter != rows.end(); iter++) {
    wxString typeCode = (*iter).ReadText(1);
    if (typeCode == _T("c")) {
      CheckConstraintModel *constraint = new CheckConstraintModel();
      constraint->name = (*iter).ReadText(0);
      constraint->expression = (*iter).ReadText(2);
      detail->checkConstraints.push_back(constraint);
    }
  }
}

void LoadRelationWork::UpdateModel(ObjectBrowserModel *model)
{
  RelationModel *relationModel = model->FindRelation(relationRef);
  relationModel->columns = detail->columns;
  relationModel->indices = detail->indices;
  relationModel->checkConstraints = detail->checkConstraints;
  relationModel->triggers = detail->triggers;
}

void LoadRelationWork::UpdateView(ObjectBrowser *ob)
{
  ob->UpdateRelation(relationRef);
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
