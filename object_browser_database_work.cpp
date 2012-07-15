#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_database_work_impl.h"
#include "object_browser.h"
#include "ssl_info.h"

#include <openssl/ssl.h>

/*
 * Database list, and other server globals.
 */
 
void RefreshDatabaseListWork::operator()()
{
  ReadServer();
  ReadRole();
  ReadDatabases();
  ReadRoles();
  ReadTablespaces();
}

void RefreshDatabaseListWork::ReadServer()
{
  serverVersionString = wxString(PQparameterStatus(conn, "server_version"), wxConvUTF8);
  serverVersion = PQserverVersion(conn);
  SSL *ssl = (SSL*) PQgetssl(conn);
  if (ssl != NULL) {
    sslInfo = new SSLInfo(ssl);
  }
  else {
    sslInfo = NULL;
  }
}

SSLInfo::SSLInfo(SSL* ssl)
{
  int version = SSL_version(ssl);
  switch (version) {
  case SSL2_VERSION:
    protocol = _T("SSLv2");
    break;
  case SSL3_VERSION:
    protocol = _T("SSLv3");
    break;
  case TLS1_VERSION:
    protocol = _T("TLSv1");
    break;
  default:
    protocol << _T("#") << version << _T("?");
    break;
  }

  const SSL_CIPHER *sslCipher = SSL_get_current_cipher(ssl);
  if (sslCipher != NULL) {
    cipher = wxString(SSL_CIPHER_get_name(sslCipher), wxConvUTF8);
    bits = SSL_CIPHER_get_bits(sslCipher, NULL);
  }

  X509 *peer = SSL_get_peer_certificate(ssl);
  if (peer != NULL) {
    X509_NAME *peerSubject = X509_get_subject_name(peer);
    char *nameBuf = X509_NAME_oneline(peerSubject, NULL, 0);
    peerDN = wxString(nameBuf, wxConvUTF8);
    OPENSSL_free(nameBuf);

    int n = X509_NAME_entry_count(peerSubject);
    for (int i = 0; i < n; i++) {
      X509_NAME_ENTRY *nameEntry = X509_NAME_get_entry(peerSubject, i);

      ASN1_STRING *nameValue = X509_NAME_ENTRY_get_data(nameEntry);
      unsigned char *nameValuePtr;
      ASN1_STRING_to_UTF8(&nameValuePtr, nameValue);
      wxString nameValueString((const char*) nameValuePtr, wxConvUTF8);
      OPENSSL_free(nameValuePtr);

      ASN1_OBJECT *nameKey = X509_NAME_ENTRY_get_object(nameEntry);
      int nid = OBJ_obj2nid(nameKey);
      if (nid != NID_undef) {
        peerDNstructure.push_back(std::pair<wxString,wxString>(wxString(OBJ_nid2sn(nid), wxConvUTF8), nameValueString));
      }
      else {
        size_t len = OBJ_obj2txt(NULL, 0, nameKey, 0);
        wxCharBuffer keyBuf(len + 1);
        OBJ_obj2txt(keyBuf.data(), len + 1, nameKey, 1);
        peerDNstructure.push_back(std::pair<wxString,wxString>(wxString(keyBuf, wxConvUTF8), nameValueString));
      }
    }
  }

  long verifyResult = SSL_get_verify_result(ssl);
  switch (verifyResult) {
  case X509_V_OK:
    verificationStatus = VERIFIED;
    break;
  case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    verificationStatus = SELF_SIGNED;
    break;
  default:
    verificationStatus = FAILED;
    break;
  }
}

void RefreshDatabaseListWork::ReadRole()
{
  QueryResults::Row row = Query(_T("Role")).UniqueResult();
  createDB = row.ReadBool(0);
  createUser = row.ReadBool(1);
  superuser = row.ReadBool(2);
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

void RefreshDatabaseListWork::UpdateModel(ObjectBrowserModel& model)
{
  ServerModel *server = model.FindServer(serverId);
  server->UpdateServerParameters(serverVersionString, serverVersion, sslInfo);
  server->UpdateDatabases(databases);
  server->UpdateRoles(roles);
  server->UpdateTablespaces(tablespaces);
  server->haveCreateDBPrivilege = createDB;
  server->haveCreateUserPrivilege = createUser;
  server->haveSuperuserStatus = superuser;
}

void RefreshDatabaseListWork::UpdateView(ObjectBrowser& ob)
{
  ob.UpdateServer(serverId, true);
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
  LoadSimpleSchemaMembers(_T("Text search dictionaries"), incoming.textSearchDictionaries);
  LoadSimpleSchemaMembers(_T("Text search parsers"), incoming.textSearchParsers);
  LoadSimpleSchemaMembers(_T("Text search templates"), incoming.textSearchTemplates);
  LoadSimpleSchemaMembers(_T("Text search configurations"), incoming.textSearchConfigurations);
}

void LoadDatabaseSchemaWork::LoadRelations() {
  QueryResults relationRows = Query(_T("Relations")).List();
  for (QueryResults::const_iterator iter = relationRows.begin(); iter != relationRows.end(); iter++) {
    RelationModel relation;
    relation.schema.oid = (*iter).ReadOid(0);
    relation.schema.name = (*iter).ReadText(1);
    relation.extension.oid = (*iter).ReadOid(2);
    relation.extension.name = (*iter).ReadText(3);
    if (!(*iter)[4].IsEmpty()) {
      relation.oid = (*iter).ReadOid(4);
      relation.name = (*iter).ReadText(5);
      wxString relkind((*iter).ReadText(6));
      relation.unlogged = (*iter).ReadBool(7);
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
    func.schema.oid = (*iter).ReadOid(0);
    func.schema.name = (*iter).ReadText(1);
    func.extension.oid = (*iter).ReadOid(2);
    func.extension.name = (*iter).ReadText(3);
    func.oid = (*iter).ReadOid(4);
    func.name = (*iter).ReadText(5);
    func.arguments = (*iter).ReadText(6);
    wxString type((*iter).ReadText(7));
    wxASSERT_MSG(functionTypeMap.count(type) > 0, type);
    func.type = functionTypeMap.find(type)->second;
    incoming.functions.push_back(func);
  }
}

template<typename T>
void LoadDatabaseSchemaWork::LoadSimpleSchemaMembers(const wxString &queryName, typename std::vector<T>& vec)
{
  const QueryResults rows = Query(queryName).List();
  for (QueryResults::const_iterator iter = rows.begin(); iter != rows.end(); iter++) {
    T obj;
    obj.schema.oid = (*iter).ReadOid(0);
    obj.schema.name = (*iter).ReadText(1);
    obj.extension.oid = (*iter).ReadOid(2);
    obj.extension.name = (*iter).ReadText(3);
    obj.oid = (*iter).ReadOid(4);
    obj.name = (*iter).ReadText(5);
    vec.push_back(obj);
  }
}

void LoadDatabaseSchemaWork::UpdateModel(ObjectBrowserModel& model)
{
  ServerModel *server = model.FindServer(databaseRef.ServerRef());
  wxASSERT(server != NULL);
  server->UpdateDatabase(incoming);
}

void LoadDatabaseSchemaWork::UpdateView(ObjectBrowser& ob)
{
  ob.UpdateDatabase(databaseRef, expandAfter);
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

template<typename T>
void PutDescriptions(const std::map<unsigned long, wxString>& descriptions, std::vector<T>& objects, int& count)
{
  for (typename std::vector<T>::iterator iter = objects.begin(); iter != objects.end(); iter++) {
    std::map<unsigned long, wxString>::const_iterator ptr = descriptions.find((*iter).oid);
    if (ptr != descriptions.end()) {
      (*iter).description = (*ptr).second;
      ++count;
    }
  }
}

void LoadDatabaseDescriptionsWork::UpdateModel(ObjectBrowserModel& model)
{
  DatabaseModel *databaseModel = model.FindDatabase(databaseRef);
  int count = 0;
  PutDescriptions(descriptions, databaseModel->relations, count);
  PutDescriptions(descriptions, databaseModel->functions, count);
  PutDescriptions(descriptions, databaseModel->textSearchDictionaries, count);
  PutDescriptions(descriptions, databaseModel->textSearchParsers, count);
  PutDescriptions(descriptions, databaseModel->textSearchTemplates, count);
  PutDescriptions(descriptions, databaseModel->textSearchConfigurations, count);
  wxLogDebug(_T("Loaded %d/%lu descriptions"), count, descriptions.size());
}

void LoadDatabaseDescriptionsWork::UpdateView(ObjectBrowser& ob)
{
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

void IndexDatabaseSchemaWork::UpdateModel(ObjectBrowserModel& model)
{
  DatabaseModel *database = model.FindDatabase(databaseRef);
  wxASSERT(database != NULL);
  database->catalogueIndex = catalogueIndex;
}

/*
 * Load a relation's details.
 */
void LoadRelationWork::operator()() {
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
  incoming.columns.reserve(attributeRows.size());
  for (QueryResults::const_iterator iter = attributeRows.begin(); iter != attributeRows.end(); iter++) {
    ColumnModel column;
    column.name = (*iter).ReadText(0);
    column.type = (*iter).ReadText(1);
    column.nullable = (*iter).ReadBool(2);
    column.hasDefault = (*iter).ReadBool(3);
    column.description = (*iter).ReadText(4);
    column.attnum = (*iter).ReadInt4(5);
    incoming.columns.push_back(column);
  }
}

void LoadRelationWork::ReadIndices() {
  QueryResults indexRows = Query(_T("Indices")).OidParam(relationRef.GetOid()).List();
  incoming.indices.reserve(indexRows.size());
  IndexModel *lastIndex = NULL;
  std::vector<int> indexColumns;
  std::vector<int>::const_iterator indexColumnsIter;
  for (QueryResults::const_iterator iter = indexRows.begin(); iter != indexRows.end(); iter++) {
    wxString indexName = (*iter)[_T("relname")];
    if (lastIndex == NULL || lastIndex->name != indexName) {
      IndexModel index;
      index.name = indexName;
      index.primaryKey = (*iter).ReadBool(_T("indisprimary"));
      index.unique = (*iter).ReadBool(_T("indisunique"));
      index.exclusion = (*iter).ReadBool(_T("indisexclusion"));
      index.clustered = (*iter).ReadBool(_T("indisclustered"));
      indexColumns = ParseInt2Vector((*iter)[_T("indkey")]);
      indexColumnsIter = indexColumns.begin();
      incoming.indices.push_back(index);
      lastIndex = &(incoming.indices.back());
    }
    wxString expression = (*iter)[_T("indexattdef")];
    lastIndex->columns.push_back(IndexModel::Column(*indexColumnsIter++, expression));
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
  incoming.triggers.reserve(triggerRows.size());
  for (QueryResults::const_iterator iter = triggerRows.begin(); iter != triggerRows.end(); iter++) {
    TriggerModel trigger;
    trigger.name = (*iter).ReadText(0);
    incoming.triggers.push_back(trigger);
  }
}

void LoadRelationWork::ReadSequences() {
  QueryResults sequenceRows = Query(_T("Sequences")).OidParam(relationRef.GetOid()).List();
  incoming.sequences.reserve(sequenceRows.size());
  for (QueryResults::const_iterator iter = sequenceRows.begin(); iter != sequenceRows.end(); iter++) {
    RelationModel sequence;
    sequence.type = RelationModel::SEQUENCE;
    sequence.schema.oid = (*iter).ReadOid(0);
    sequence.schema.name = (*iter).ReadText(1);
    sequence.oid = (*iter).ReadOid(2);
    sequence.name = (*iter).ReadText(3);
    sequence.owningColumn = (*iter).ReadInt4(4);
    incoming.sequences.push_back(sequence);
  }
}

void LoadRelationWork::ReadConstraints() {
  QueryResults rows = Query(_T("Constraints")).OidParam(relationRef.GetOid()).List();
  for (QueryResults::const_iterator iter = rows.begin(); iter != rows.end(); iter++) {
    wxString typeCode = (*iter).ReadText(1);
    if (typeCode == _T("c")) {
      CheckConstraintModel constraint;
      constraint.name = (*iter).ReadText(0);
      constraint.expression = (*iter).ReadText(2);
      incoming.checkConstraints.push_back(constraint);
    }
  }
}

void LoadRelationWork::UpdateModel(ObjectBrowserModel& model)
{
  RelationModel *relationModel = model.FindRelation(relationRef);
  relationModel->columns = incoming.columns;
  relationModel->indices = incoming.indices;
  relationModel->triggers = incoming.triggers;
  relationModel->sequences = incoming.sequences;
  relationModel->checkConstraints = incoming.checkConstraints;
}

void LoadRelationWork::UpdateView(ObjectBrowser& ob)
{
  ob.UpdateRelation(relationRef);
}

void DropDatabaseWork::operator()()
{
  wxString sql;
  sql << _T("DROP DATABASE ") << QuoteIdent(dbname);
  owner->DatabaseWork::DoCommand(sql);
}

void DropDatabaseWork::UpdateModel(ObjectBrowserModel& model)
{
  ServerModel *server = model.FindServer(serverId);
  wxASSERT(server != NULL);
  server->RemoveDatabase(dbname);
}

void DropDatabaseWork::UpdateView(ObjectBrowser& ob)
{
  ob.UpdateServer(serverId, false);
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
