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
 
void LoadServerWork::DoManagedWork()
{
  ReadServer();
  ReadCurrentRole();
  LoadThings(_T("Databases"), databases, ReadDatabase);
  LoadThings(_T("Roles"), roles, ReadRole);
  LoadThings(_T("Tablespaces"), tablespaces, ReadTablespace);
  std::sort(databases.begin(), databases.end(), ObjectModel::CollateByName);
  std::sort(roles.begin(), roles.end(), ObjectModel::CollateByName);
  std::sort(tablespaces.begin(), tablespaces.end(), ObjectModel::CollateByName);
}

void LoadServerWork::ReadServer()
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

void LoadServerWork::ReadCurrentRole()
{
  QueryResults::Row row = Query(_T("Role")).UniqueResult();
  createDB = row.ReadBool(0);
  createUser = row.ReadBool(1);
  superuser = row.ReadBool(2);
  rolename = row.ReadText(3);
}

DatabaseModel LoadServerWork::ReadDatabase(const QueryResults::Row& row)
{
  DatabaseModel database;
  database.oid = row.ReadOid(0);
  database.name = row.ReadText(1);
  database.isTemplate = row.ReadBool(2);
  database.allowConnections = row.ReadBool(3);
  database.havePrivsToConnect = row.ReadBool(4);
  database.description = row.ReadText(5);
  database.owner = row.ReadText(6);
  return database;
}

RoleModel LoadServerWork::ReadRole(const QueryResults::Row& row)
{
  RoleModel role;
  role.oid = row.ReadOid(0);
  role.name = row.ReadText(1);
  role.canLogin = row.ReadBool(2);
  role.superuser = row.ReadBool(3);
  role.description = row.ReadText(4);
  return role;
}

TablespaceModel LoadServerWork::ReadTablespace(const QueryResults::Row& row)
{
  TablespaceModel spc;
  spc.oid = row.ReadOid(0);
  spc.name = row.ReadText(1);
  spc.location = row.ReadText(2);
  return spc;
}

void LoadServerWork::UpdateModel(ObjectBrowserModel& model)
{
  ServerModel *server = model.FindServer(serverId);
  server->UpdateServerParameters(serverVersionString, serverVersion, sslInfo);
  server->UpdateDatabases(databases);
  server->UpdateRoles(roles);
  server->UpdateTablespaces(tablespaces);
  server->haveCreateDBPrivilege = createDB;
  server->haveCreateUserPrivilege = createUser;
  server->haveSuperuserStatus = superuser;
  server->rolename = rolename;
}

void LoadServerWork::UpdateView(ObjectBrowser& ob)
{
  ob.UpdateServer(serverId, true);
}

/*
 * Schema of a single database
 */

std::map<wxString, RelationModel::Type> LoadDatabaseWork::InitRelationTypeMap()
{
  std::map<wxString, RelationModel::Type> typemap;
  typemap[_T("r")] = RelationModel::TABLE;
  typemap[_T("v")] = RelationModel::VIEW;
  typemap[_T("S")] = RelationModel::SEQUENCE;
  return typemap;
}

const std::map<wxString, RelationModel::Type> LoadDatabaseWork::relationTypeMap = InitRelationTypeMap();

std::map<wxString, FunctionModel::Type> LoadDatabaseWork::InitFunctionTypeMap()
{
  std::map<wxString, FunctionModel::Type> typemap;
  typemap[_T("f")] = FunctionModel::SCALAR;
  typemap[_T("ft")] = FunctionModel::TRIGGER;
  typemap[_T("fs")] = FunctionModel::RECORDSET;
  typemap[_T("fa")] = FunctionModel::AGGREGATE;
  typemap[_T("fw")] = FunctionModel::WINDOW;
  return typemap;
}

const std::map<wxString, FunctionModel::Type> LoadDatabaseWork::functionTypeMap = InitFunctionTypeMap();

std::map<wxString, OperatorModel::Kind> LoadDatabaseWork::InitOperatorKindMap()
{
  std::map<wxString, OperatorModel::Kind> kindmap;
  kindmap[_T("l")] = OperatorModel::LEFT_UNARY;
  kindmap[_T("r")] = OperatorModel::RIGHT_UNARY;
  kindmap[_T("b")] = OperatorModel::BINARY;
  return kindmap;
}

const std::map<wxString, OperatorModel::Kind> LoadDatabaseWork::operatorKindMap = InitOperatorKindMap();

template <class T>
const T& LoadDatabaseWork::InternalLookup(const typename std::map<Oid, T>& table, Oid oid) const
{
  typename std::map<Oid,T>::const_iterator ptr = table.find(oid);
  wxASSERT_MSG(ptr != table.end(), wxString::Format(_T("%u not found in lookup table"), oid));
  return ptr->second;
}

template<class T, class InputIterator>
void LoadDatabaseWork::PopulateInternalLookup(typename std::map<Oid, T>& table, InputIterator first, InputIterator last)
{
  while (first != last) {
    table[(*first).oid] = *first;
    ++first;
  }
}

void LoadDatabaseWork::DoManagedWork() {
  incoming.oid = databaseRef.GetOid();

  LoadThings(_T("Schemas"), incoming.schemas, ReadSchema);
  LoadThings(_T("Extensions"), incoming.extensions, ReadExtension);

  PopulateInternalLookup(schemas, incoming.schemas.begin(), incoming.schemas.end());
  PopulateInternalLookup(extensions, incoming.extensions.begin(), incoming.extensions.end());

  LoadThings(_T("Relations"), incoming.relations);
  LoadThings(_T("Functions"), incoming.functions);
  LoadThings(_T("Text search dictionaries"), incoming.textSearchDictionaries);
  LoadThings(_T("Text search parsers"), incoming.textSearchParsers);
  LoadThings(_T("Text search templates"), incoming.textSearchTemplates);
  LoadThings(_T("Text search configurations"), incoming.textSearchConfigurations);
  LoadThings(_T("Types"), incoming.types);
  LoadThings(_T("Operators"), incoming.operators);
}

SchemaModel LoadDatabaseWork::ReadSchema(const QueryResults::Row& row)
{
  SchemaModel schema;
  schema.oid = row.ReadOid(0);
  schema.name = row.ReadText(1);
  schema.accessible = row.ReadBool(2);
  return schema;
}

ExtensionModel LoadDatabaseWork::ReadExtension(const QueryResults::Row& row)
{
  ExtensionModel extension;
  extension.oid = row.ReadOid(0);
  extension.name = row.ReadText(1);
  return extension;
}

template<>
RelationModel LoadDatabaseWork::Mapper<RelationModel>::operator()(const QueryResults::Row& row)
{
  RelationModel relation;
  relation.schema = Schema(row.ReadOid(0));
  relation.extension = Extension(row.ReadOid(1));
  if (!row[4].IsEmpty()) {
    relation.oid = row.ReadOid(2);
    relation.name = row.ReadText(3);
    wxString relkind(row.ReadText(4));
    relation.unlogged = row.ReadBool(5);
    wxASSERT_MSG(relationTypeMap.count(relkind) > 0, relkind);
    relation.type = relationTypeMap.find(relkind)->second;
  }
  return relation;
}

template<>
FunctionModel LoadDatabaseWork::Mapper<FunctionModel>::operator()(const QueryResults::Row& row)
{
  FunctionModel func;
  func.schema = Schema(row.ReadOid(0));
  func.extension = Extension(row.ReadOid(1));
  func.oid = row.ReadOid(2);
  func.name = row.ReadText(3);
  func.arguments = row.ReadText(4);
  wxString type(row.ReadText(5));
  wxASSERT_MSG(functionTypeMap.count(type) > 0, type);
  func.type = functionTypeMap.find(type)->second;
  return func;
}

template<>
OperatorModel LoadDatabaseWork::Mapper<OperatorModel>::operator()(const QueryResults::Row& row)
{
  OperatorModel obj;
  obj.schema = Schema(row.ReadOid(0));
  obj.extension = Extension(row.ReadOid(1));
  obj.oid = row.ReadOid(2);
  obj.name = row.ReadText(3);
  obj.leftType = row.ReadOid(4);
  obj.rightType = row.ReadOid(5);
  obj.resultType = row.ReadOid(6);
  wxString kind = row.ReadText(7);
  wxASSERT_MSG(operatorKindMap.count(kind) > 0, kind);
  obj.kind = operatorKindMap.find(kind)->second;

  return obj;
}

template<typename T>
T LoadDatabaseWork::Mapper<T>::operator()(const QueryResults::Row& row)
{
  T obj;
  obj.schema = Schema(row.ReadOid(0));
  obj.extension = Extension(row.ReadOid(1));
  obj.oid = row.ReadOid(2);
  obj.name = row.ReadText(3);
  return obj;
}

void LoadDatabaseWork::UpdateModel(ObjectBrowserModel& model)
{
  ServerModel *server = model.FindServer(databaseRef.ServerRef());
  wxASSERT(server != NULL);
  server->UpdateDatabase(incoming);
}

void LoadDatabaseWork::UpdateView(ObjectBrowser& ob)
{
  ob.UpdateDatabase(databaseRef, expandAfter);
}

/*
 * All database object descriptions.
 */
void LoadDatabaseDescriptionsWork::DoManagedWork()
{
  QueryResults rs = Query(_T("Object Descriptions")).List();
  for (QueryResults::rows_iterator iter = rs.Rows().begin(); iter != rs.Rows().end(); iter++) {
    unsigned long oid;
    wxString description;
    oid = (*iter).ReadOid(0);
    description = (*iter).ReadText(1);
    descriptions[oid] = description;
  }
}

template<typename InputIterator>
unsigned LoadDatabaseDescriptionsWork::PutDescriptions(InputIterator iter, InputIterator last) const
{
  unsigned count = 0;
  while (iter != last) {
    std::map<Oid, wxString>::const_iterator ptr = descriptions.find((*iter).oid);
    if (ptr != descriptions.end()) {
      (*iter).description = (*ptr).second;
      ++count;
    }
    ++iter;
  }
  return count;
}

void LoadDatabaseDescriptionsWork::UpdateModel(ObjectBrowserModel& model)
{
  DatabaseModel *databaseModel = model.FindDatabase(databaseRef);
  unsigned count = 0;
  count += PutDescriptions(databaseModel->relations.begin(), databaseModel->relations.end());
  count += PutDescriptions(databaseModel->functions.begin(), databaseModel->functions.end());
  count += PutDescriptions(databaseModel->textSearchDictionaries.begin(), databaseModel->textSearchDictionaries.end());
  count += PutDescriptions(databaseModel->textSearchParsers.begin(), databaseModel->textSearchParsers.end());
  count += PutDescriptions(databaseModel->textSearchTemplates.begin(), databaseModel->textSearchTemplates.end());
  count += PutDescriptions(databaseModel->textSearchConfigurations.begin(), databaseModel->textSearchConfigurations.end());
  wxLogDebug(_T("Loaded %u/%lu descriptions"), count, descriptions.size());
}

void LoadDatabaseDescriptionsWork::UpdateView(ObjectBrowser& ob)
{
}

/**
 * Compile database catalogue index.
 */
std::map<wxString, CatalogueIndex::Type> IndexDatabaseSchemaWork::InitTypeMap()
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

const std::map<wxString, CatalogueIndex::Type> IndexDatabaseSchemaWork::typeMap = InitTypeMap();

void IndexDatabaseSchemaWork::DoManagedWork() {
  QueryResults rs = Query(_T("IndexSchema")).List();
  catalogueIndex = new CatalogueIndex();
  catalogueIndex->Begin();
  for (QueryResults::rows_iterator iter = rs.Rows().begin(); iter != rs.Rows().end(); iter++) {
    Oid entityId = (*iter).ReadOid(0);
    wxString typeString = (*iter).ReadText(1);
    wxString symbol = (*iter).ReadText(2);
    wxString disambig = (*iter).ReadText(3);
    wxString extension = (*iter).ReadText(4);
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
    catalogueIndex->AddDocument(CatalogueIndex::Document(entityId, entityType, systemObject, extension, symbol, disambig));
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
void LoadRelationWork::DoManagedWork()
{
  LoadThings(_T("Columns"), incoming.columns, ReadColumn);
  if (relationType == RelationModel::TABLE) {
    LoadIndices();
    LoadThings(_T("Triggers"), incoming.triggers, ReadTrigger);
    LoadThings(_T("Sequences"), incoming.sequences, ReadSequence);
    LoadThings(_T("Constraints"), LoadConstraint(*this));
  }
}

ColumnModel LoadRelationWork::ReadColumn(const QueryResults::Row& row)
{
  ColumnModel column;
  column.name = row.ReadText(0);
  column.type = row.ReadText(1);
  column.nullable = row.ReadBool(2);
  column.hasDefault = row.ReadBool(3);
  column.description = row.ReadText(4);
  column.attnum = row.ReadInt4(5);
  return column;
}

void LoadRelationWork::LoadIndices()
{
  QueryResults indexRows = Query(_T("Indices")).OidParam(relationRef.GetOid()).List();
  incoming.indices.reserve(indexRows.Rows().size());
  IndexModel *lastIndex = NULL;
  std::vector<int> indexColumns;
  std::vector<int>::const_iterator indexColumnsIter;
  for (QueryResults::rows_iterator iter = indexRows.Rows().begin(); iter != indexRows.Rows().end(); iter++) {
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

TriggerModel LoadRelationWork::ReadTrigger(const QueryResults::Row& row)
{
  TriggerModel trigger;
  trigger.name = row.ReadText(0);
  return trigger;
}

RelationModel LoadRelationWork::ReadSequence(const QueryResults::Row& row)
{
  RelationModel sequence;
  sequence.type = RelationModel::SEQUENCE;
  sequence.schema.oid = row.ReadOid(0);
  sequence.extension.oid = row.ReadOid(1);
  sequence.oid = row.ReadOid(2);
  sequence.name = row.ReadText(3);
  sequence.owningColumn = row.ReadInt4(4);
  return sequence;
}

void LoadRelationWork::LoadConstraint::operator()(const QueryResults::Row& row)
{
  wxString typeCode = row.ReadText(1);
  if (typeCode == _T("c")) {
    CheckConstraintModel constraint;
    constraint.name = row.ReadText(0);
    constraint.expression = row.ReadText(2);
    checkConstraints.push_back(constraint);
  }
}

void LoadRelationWork::UpdateModel(ObjectBrowserModel& model)
{
  DatabaseModel *db = model.FindDatabase(relationRef.DatabaseRef());
  RelationModel *relationModel = model.FindRelation(relationRef);
  relationModel->columns = incoming.columns;
  relationModel->indices = incoming.indices;
  relationModel->triggers = incoming.triggers;
  relationModel->sequences = incoming.sequences;
  relationModel->checkConstraints = incoming.checkConstraints;

  std::for_each(relationModel->sequences.begin(), relationModel->sequences.end(), LinkSequence(db));
}

void LoadRelationWork::LinkSequence::operator()(RelationModel& sequence) const
{
  const SchemaModel *schema = static_cast<const SchemaModel*>(db->FindObject(ObjectModelReference(*db, ObjectModelReference::PG_NAMESPACE, sequence.schema.oid)));
  wxASSERT(schema != NULL);
  sequence.schema = *schema;

  if (sequence.extension.oid != InvalidOid) {
    const ExtensionModel *extension = static_cast<const ExtensionModel*>(db->FindObject(ObjectModelReference(*db, ObjectModelReference::PG_EXTENSION, sequence.extension.oid)));
    wxASSERT(extension != NULL);
    sequence.extension = *extension;
  }
}

void LoadRelationWork::UpdateView(ObjectBrowser& ob)
{
  ob.UpdateRelation(relationRef);
}

void DropDatabaseWork::DoManagedWork()
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
