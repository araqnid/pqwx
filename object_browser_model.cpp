#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "pqwx.h"
#include "object_browser_model.h"
#include "object_browser_work.h"
#include "object_browser_database_work_impl.h"
#include "ssl_info.h"

DEFINE_LOCAL_EVENT_TYPE(PQWX_ObjectBrowserWorkFinished)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ObjectBrowserWorkCrashed)
DEFINE_LOCAL_EVENT_TYPE(PQWX_RescheduleObjectBrowserWork)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ObjectBrowserConnectionMade)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ObjectBrowserConnectionFailed)
DEFINE_LOCAL_EVENT_TYPE(PQWX_ObjectBrowserConnectionNeedsPassword)

/**
 * Implementation of DatabaseWork that deals with passing results back to the GUI thread.
 */
class ObjectBrowserDatabaseWork : public DatabaseWorkWithDictionary {
public:
  ObjectBrowserDatabaseWork(wxEvtHandler *dest, ObjectBrowserModel *model, ObjectBrowserManagedWork *work) : DatabaseWorkWithDictionary(work->sqlDictionary), dest(dest), model(model), work(work) {}
  void DoWork() {
    TransactionBoundary txn(work->txMode, this);
    work->owner = this;
    work->conn = conn;
    work->DoManagedWork();
  }
  void NotifyFinished() {
    wxLogDebug(_T("%p: object browser work finished, notifying GUI thread"), work);
    wxCommandEvent event(PQWX_ObjectBrowserWorkFinished);
    event.SetClientData(work);
    dest->AddPendingEvent(event);
  }
  void NotifyCrashed() {
    wxLogDebug(_T("%p: object browser work crashed with no known exception, notifying GUI thread"), work);
    wxCommandEvent event(PQWX_ObjectBrowserWorkCrashed);
    event.SetClientData(work);
    dest->AddPendingEvent(event);
  }
  void NotifyCrashed(const std::exception& e) {
    const PgLostConnection* lostConnection = dynamic_cast<const PgLostConnection*>(&e);
    if (lostConnection != NULL) {
      wxLogDebug(_T("%p: object browser work indicated connection lost, rescheduling"), work);
      Reschedule();
      return;
    }

    wxLogDebug(_T("%p: object browser work crashed with some exception, notifying GUI thread"), work);

    const PgQueryRelatedException* query = dynamic_cast<const PgQueryRelatedException*>(&e);
    if (query != NULL) {
      if (query->IsFromNamedQuery()) {
        work->crashedQueryName = query->GetQueryName();
        work->crashMessage = _T("While running query \"") + work->crashedQueryName + _T("\":\n");
      }
      else
        work->crashMessage = _T("While running dynamic SQL:\n\n") + query->GetSql() + _T("\n\n");
      const PgQueryFailure* queryError = dynamic_cast<const PgQueryFailure*>(&e);
      if (queryError != NULL) {
        const PgError& details = queryError->GetDetails();
        work->crashMessage += details.GetSeverity() + _T(": ") + details.GetPrimary();
        if (!details.GetDetail().empty())
          work->crashMessage += _T("\nDETAIL: ") + details.GetDetail();
        if (!details.GetHint().empty())
          work->crashMessage += _T("\nHINT: ") + details.GetHint();
        for (std::vector<wxString>::const_iterator iter = details.GetContext().begin(); iter != details.GetContext().end(); iter++) {
          work->crashMessage += _T("\nCONTEXT: ") + *iter;
        }
        work->crashPgError.reset(new PgError(details));
      }
      else
        work->crashMessage += wxString(e.what(), wxConvUTF8);
    }
    else {
      work->crashMessage = wxString(e.what(), wxConvUTF8);
    }

    wxCommandEvent event(PQWX_ObjectBrowserWorkCrashed);
    event.SetClientData(work);
    dest->AddPendingEvent(event);
  }
  void NotifyLostConnection()
  {
    wxLogDebug(_T("%p: object browser work abandoned, rescheduling"), work);
    Reschedule();
  }

private:
  wxEvtHandler * const dest;
  ObjectBrowserModel * const model;
  ObjectBrowserManagedWork * const work;

  void Reschedule()
  {
    wxCommandEvent event(PQWX_RescheduleObjectBrowserWork);
    event.SetClientData(work);
    model->AddPendingEvent(event);
  }

  class TransactionBoundary {
  public:
    TransactionBoundary(ObjectBrowserManagedWork::TxMode mode, DatabaseWork *work) : mode(mode), work(work), began(FALSE)
    {
      if (mode != ObjectBrowserManagedWork::NO_TRANSACTION) {
        if (mode == ObjectBrowserManagedWork::READ_ONLY)
          work->DoCommand("BEGIN ISOLATION LEVEL REPEATABLE READ READ ONLY");
        else
          work->DoCommand("BEGIN");
        began = TRUE;
      }
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
    ObjectBrowserManagedWork::TxMode mode;
    DatabaseWork *work;
    bool began;
  };

};

class ObjectBrowserConnectionCallback : public ConnectionCallback {
public:
  ObjectBrowserConnectionCallback(ObjectBrowserModel& model, const ObjectModelReference& ref) : model(model), ref(ref) {}

  void OnConnection(bool usedPassword)
  {
    PQWXObjectBrowserModelEvent event(ref, PQWX_ObjectBrowserConnectionMade);
    model.AddPendingEvent(event);
  }

  void OnConnectionFailed(const PgError& error)
  {
    PQWXObjectBrowserModelEvent event(ref, PQWX_ObjectBrowserConnectionFailed);
    event.SetString(error.GetPrimary());
    model.AddPendingEvent(event);
  }

  void OnConnectionNeedsPassword()
  {
    PQWXObjectBrowserModelEvent event(ref, PQWX_ObjectBrowserConnectionNeedsPassword);
    model.AddPendingEvent(event);
  }
private:
  ObjectBrowserModel& model;
  const ObjectModelReference ref;
};

BEGIN_EVENT_TABLE(ObjectBrowserModel, wxEvtHandler)
  PQWX_OBJECT_BROWSER_WORK_FINISHED(wxID_ANY, ObjectBrowserModel::OnWorkFinished)
  PQWX_OBJECT_BROWSER_WORK_CRASHED(wxID_ANY, ObjectBrowserModel::OnWorkCrashed)
  PQWX_RESCHEDULE_OBJECT_BROWSER_WORK(wxID_ANY, ObjectBrowserModel::OnRescheduleWork)
  PQWX_OBJECT_BROWSER_CONNECTION_MADE(wxID_ANY, ObjectBrowserModel::OnConnectionMade)
  PQWX_OBJECT_BROWSER_CONNECTION_FAILED(wxID_ANY, ObjectBrowserModel::OnConnectionFailed)
  PQWX_OBJECT_BROWSER_CONNECTION_NEEDS_PASSWORD(wxID_ANY, ObjectBrowserModel::OnConnectionNeedsPassword)
  EVT_TIMER(ObjectBrowserModel::TIMER_MAINTAIN, ObjectBrowserModel::OnTimerTick)
END_EVENT_TABLE()

void ObjectBrowserModel::SubmitServerWork(ServerModel *server, ObjectBrowserManagedWork *work)
{
  ConnectAndAddWork(server->GetServerAdminDatabaseRef(), server->GetServerAdminConnection(), new ObjectBrowserDatabaseWork(work->dest == NULL ? this : work->dest, this, work));
}

void ObjectBrowserModel::SubmitDatabaseWork(DatabaseModel *database, ObjectBrowserManagedWork *work)
{
  ConnectAndAddWork(*database, database->GetDatabaseConnection(), new ObjectBrowserDatabaseWork(work->dest == NULL ? this : work->dest, this, work));
}

void ObjectBrowserModel::ConnectAndAddWork(const ObjectModelReference& ref, DatabaseConnection *db, DatabaseWork *work)
{
  // still a bodge. what if the database connection fails? need to clean up any work added in the meantime...
  if (!db->IsConnected()) {
    wxBeginBusyCursor();
    db->Connect(new ObjectBrowserConnectionCallback(*this, ref));
    SetupDatabaseConnection(ref, db);
  }
  db->AddWork(work);
}

void ObjectBrowserModel::OnConnectionMade(PQWXObjectBrowserModelEvent& e)
{
  wxLogDebug(_T("Object browser connection made"));
  wxEndBusyCursor();
}

void ObjectBrowserModel::OnConnectionFailed(PQWXObjectBrowserModelEvent& e)
{
  wxLogError(_T("Object browser connection failed: %s"), e.GetString().c_str());
  wxEndBusyCursor();
}

void ObjectBrowserModel::OnConnectionNeedsPassword(PQWXObjectBrowserModelEvent& e)
{
  wxLogDebug(_T("Object browser connection needs password (TODO)"));
  wxEndBusyCursor();
}

void ObjectBrowserModel::OnWorkFinished(wxCommandEvent &e) {
  ObjectBrowserWork *work = static_cast<ObjectBrowserWork*>(e.GetClientData());

  wxLogDebug(_T("%p: work finished (received by model)"), work);
  work->UpdateModel(*this);

  for (std::list<ObjectBrowser*>::iterator iter = views.begin(); iter != views.end(); iter++) {
    wxLogDebug(_T("%p: updating view %p"), work, *iter);
    work->UpdateView(**iter);
  }

  if (work->completion != NULL) {
    wxLogDebug(_T("%p: calling OnCompletion on completion callback"), work);
    work->completion->OnCompletion();
  }

  delete work;
}

void ObjectBrowserModel::OnWorkCrashed(wxCommandEvent &e)
{
  ObjectBrowserWork *work = static_cast<ObjectBrowserWork*>(e.GetClientData());

  wxLogDebug(_T("%p: work crashed (received by model)"), work);
  if (!work->GetCrashMessage().empty()) {
    wxLogError(_T("%s\n%s"), _("An unexpected error occurred interacting with the database. Failure will ensue."), work->GetCrashMessage().c_str());
  }
  else {
    wxLogError(_T("%s"), _("An unexpected and unidentified error occurred interacting with the database. Failure will ensue."));
  }

  if (work->completion != NULL) {
    wxLogDebug(_T("%p: calling OnCrash on completion callback"), work);
    work->completion->OnCrash();
  }
  delete work;
}

void ObjectBrowserModel::OnRescheduleWork(wxCommandEvent &e)
{
  ObjectBrowserManagedWork *work = static_cast<ObjectBrowserManagedWork*>(e.GetClientData());
  wxLogDebug(_T("%p: work needs rescheduling (received by model)"), work);

  const ObjectModelReference& dbRef = work->database;
  wxLogDebug(_T("%p: reschedule on %s"), work, dbRef.Identify().c_str());
  wxASSERT_MSG(dbRef.GetObjectClass() == ObjectModelReference::PG_DATABASE, dbRef.Identify());

  // special hack to reschedule these as "server work"
  if (dbRef.GetOid() == InvalidOid) {
    ServerModel *server = FindServer(dbRef.GetServerId());
    wxASSERT(server != NULL);
    SubmitServerWork(server, work);
  }
  else {
    DatabaseModel *database = FindDatabase(dbRef);
    wxASSERT(database != NULL);
    SubmitDatabaseWork(database, work);
  }
}

void ObjectBrowserModel::OnTimerTick(wxTimerEvent &e)
{
  Maintain();
}

void ObjectBrowserModel::Maintain()
{
  for (std::list<ServerModel>::iterator serverIter = servers.begin(); serverIter != servers.end(); serverIter++) {
    (*serverIter).Maintain();
  }
}

wxString ObjectModelReference::Identify() const
{
  wxString buf;
  buf << serverId;
  if (database != InvalidOid) {
    buf << _T("|database=") << database;
    if (regclass != PG_DATABASE) {
      buf << _T("|regclass=") << regclass << _T("|oid=") << oid;
      if (subid)
        buf << _T("|objsubid=") << subid;
    }
  }
  return buf;
}

ServerModel *ObjectBrowserModel::FindServerById(const wxString &serverId)
{
  std::list<ServerModel>::iterator ptr = std::find_if(servers.begin(), servers.end(), ServerIdEquals(serverId));
  return ptr != servers.end() ? &(*ptr) : NULL;
}

ObjectModel* ObjectBrowserModel::FindObject(const ObjectModelReference& ref)
{
  ServerModel *server = FindServerById(ref.GetServerId());
  if (ref.GetObjectClass() == InvalidOid)
    return server;
  else
    return server->FindObject(ref);
}

ServerModel* ObjectBrowserModel::AddServerConnection(const ServerConnection &server, DatabaseConnection *db)
{
  if (db) {
    servers.push_back(ServerModel(server, db));
    if (db->IsConnected()) {
      SetupDatabaseConnection(servers.back().GetServerAdminDatabaseRef(), db);
    }
  }
  else {
    servers.push_back(ServerModel(server));
  }

  return &(servers.back());
}

void ObjectBrowserModel::SetupDatabaseConnection(const ObjectModelReference& ref, DatabaseConnection *db)
{
  db->AddWork(new ObjectBrowserDatabaseWork(this, this, new SetupDatabaseConnectionWork(ref)));
}

void ObjectBrowserModel::Dispose()
{
  wxLogDebug(_T("Disposing of ObjectBrowser- sending disconnection request to all server databases"));
  std::vector<DatabaseConnection*> disconnecting;
  for (std::list<ServerModel>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    (*iter).BeginDisconnectAll(disconnecting);
  }
  wxLogDebug(_T("Disposing of ObjectBrowser- waiting for %lu database connections to terminate"), disconnecting.size());
  for (std::vector<DatabaseConnection*>::const_iterator iter = disconnecting.begin(); iter != disconnecting.end(); iter++) {
    DatabaseConnection *db = *iter;
    wxLogDebug(_T(" Waiting for database connection %s to exit"), db->Identification().c_str());
    db->WaitUntilClosed();
  }
  wxLogDebug(_T("Disposing of ObjectBrowser- disposing of servers"));
  for (std::list<ServerModel>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    (*iter).Dispose();
  }
  wxLogDebug(_T("Disposing of ObjectBrowser- clearing server list"));
  servers.clear();
}

void ObjectBrowserModel::RemoveServer(const wxString& serverId)
{
  for (std::list<ServerModel>::iterator iter = servers.begin(); iter != servers.end(); iter++) {
    if ((*iter).Identification() == serverId) {
      (*iter).Dispose(); // still does nasty synchronous disconnect for now
      servers.erase(iter);
      return;
    }
  }
}

DatabaseModel* ObjectBrowserModel::FindAdminDatabase(const wxString& serverId)
{
  ServerModel* server = FindServerById(serverId);
  wxASSERT(server != NULL);
  return server->FindAdminDatabase();
}

DatabaseModel *ObjectBrowserModel::FindDatabase(const ServerConnection &server, const wxString &dbname)
{
  ServerModel *serverModel = FindServer(server);
  if (serverModel == NULL) return NULL;
  return serverModel->FindDatabase(dbname);
}

DatabaseModel *ObjectBrowserModel::FindDatabase(const ObjectModelReference &ref)
{
  ServerModel *serverModel = FindServer(ref.ServerRef());
  if (serverModel == NULL) return NULL;
  return serverModel->FindDatabase(ref);
}

DatabaseConnection* ServerModel::GetDatabaseConnection(const wxString &dbname)
{
  std::map<wxString, DatabaseConnection*>::const_iterator iter = connections.find(dbname);
  if (iter != connections.end()) {
    DatabaseConnection *db = iter->second;
    if (db->IsAcceptingWork()) {
      wxLogDebug(_T("Using existing connection %s"), db->Identification().c_str());
      return db;
    }
    wxLogDebug(_T("Cleaning stale connection %s"), db->Identification().c_str());
    db->Dispose();
    delete db;
    connections.erase(dbname);
  }
  DatabaseConnection *db = new DatabaseConnection(conninfo, dbname);
  wxLogDebug(_T("Allocating connection to %s"), db->Identification().c_str());
  for (std::map<wxString, DatabaseConnection*>::const_iterator iter = connections.begin(); iter != connections.end(); iter++) {
    if (iter->second->IsConnected()) {
      wxLogDebug(_T(" Closing existing connection to %s"), iter->second->Identification().c_str());
      iter->second->BeginDisconnection();
    }
  }
  connections[dbname] = db;
  return db;
}

void ServerModel::Maintain()
{
  for (std::map<wxString, DatabaseConnection*>::iterator iter = connections.begin(); iter != connections.end(); iter++) {
    DatabaseConnection *db = iter->second;
    if (!db->IsAcceptingWork()) {
      wxLogDebug(_T("Cleaning stale connection %s"), db->Identification().c_str());
      db->Dispose();
      delete db;
      connections.erase(iter->first);
    }
  }
}

void ServerModel::BeginDisconnectAll(std::vector<DatabaseConnection*> &disconnecting)
{
  for (std::map<wxString, DatabaseConnection*>::iterator iter = connections.begin(); iter != connections.end(); iter++) {
    DatabaseConnection *db = iter->second;
    if (db->BeginDisconnection()) {
      wxLogDebug(_T(" Sent disconnect request to %s"), db->Identification().c_str());
      disconnecting.push_back(db);
    }
    else {
      wxLogDebug(_T(" Already disconnected from %s"), db->Identification().c_str());
    }
  }
}

void ServerModel::Dispose()
{
  wxLogDebug(_T("Disposing of server %s"), Identification().c_str());
  std::vector<DatabaseConnection*> disconnecting;
  BeginDisconnectAll(disconnecting);
  for (std::vector<DatabaseConnection*>::const_iterator iter = disconnecting.begin(); iter != disconnecting.end(); iter++) {
    DatabaseConnection *db = *iter;
    wxLogDebug(_T(" Waiting for database connection %s to exit"), db->Identification().c_str());
    db->WaitUntilClosed();
  }
  for (std::map<wxString, DatabaseConnection*>::iterator iter = connections.begin(); iter != connections.end(); iter++) {
    DatabaseConnection *db = iter->second;
    db->Dispose();
    delete db;
  }
  connections.clear();
}

void ServerModel::DropDatabase(DatabaseModel *database)
{
  // Detect script windows still connected to this database?
  // This should all be done in the background somehow
  std::map<wxString, DatabaseConnection*>::const_iterator iter = connections.find(database->name);
  if (iter != connections.end()) {
    DatabaseConnection *db = iter->second;
    if (db->IsAcceptingWork()) {
      wxLogDebug(_T("Closing database connection to %s before droppping"), db->Identification().c_str());
      db->Dispose();
      delete db;
    }
    connections.erase(database->name);
  }
  // execute drop script...
  SubmitWork(new DropDatabaseWork(GetServerAdminDatabaseRef(), database->name));
}

DatabaseModel *ServerModel::FindDatabase(const wxString &dbname)
{
  std::vector<DatabaseModel>::iterator ptr = std::find_if(databases.begin(), databases.end(), NameEquals<DatabaseModel>(dbname));
  return ptr != databases.end() ? &(*ptr) : NULL;
}

DatabaseModel *ServerModel::FindDatabaseByOid(Oid oid)
{
  std::vector<DatabaseModel>::iterator ptr = std::find_if(databases.begin(), databases.end(), OidEquals<DatabaseModel>(oid));
  return ptr != databases.end() ? &(*ptr) : NULL;
}

void ServerModel::RemoveDatabase(const wxString &dbname)
{
  std::vector<DatabaseModel>::iterator ptr = std::find_if(databases.begin(), databases.end(), NameEquals<DatabaseModel>(dbname));
  if (ptr != databases.end())
    databases.erase(ptr);
}

ObjectModel *ServerModel::FindObject(const ObjectModelReference& ref)
{
  DatabaseModel *database = FindDatabaseByOid(ref.GetDatabase());
  if (database == NULL) return NULL;
  if (ref.GetObjectClass() == ObjectModelReference::PG_DATABASE)
    return database;
  else
    return database->FindObject(ref);
}

void ServerModel::UpdateServerParameters(const wxString& serverVersionString_, int serverVersion_, SSLInfo* sslInfo_, ReplicationState replication_)
{
  serverVersionString = serverVersionString_;
  serverVersion = serverVersion_;
  if (sslInfo != NULL) delete sslInfo;
  sslInfo = sslInfo_;
  replication = replication_;
}

ServerModel::~ServerModel()
{
  if (sslInfo != NULL) delete sslInfo;
}

void ServerModel::SubmitWork(ObjectBrowserManagedWork *work)
{
  ObjectBrowserModel& model = ::wxGetApp().GetObjectBrowserModel();
  model.SubmitServerWork(this, work);
}

void ServerModel::Load()
{
  SubmitWork(new LoadServerWork(Identification()));
}

void ServerModel::UpdateDatabases(const std::vector<DatabaseModel>& incoming)
{
  databases.clear();
  databases.reserve(incoming.size());
  std::copy(incoming.begin(), incoming.end(), std::back_inserter(databases));
  std::for_each(databases.begin(), databases.end(), AttachServer(this));
}

void ServerModel::UpdateDatabase(const DatabaseModel& incoming)
{
  for (std::vector<DatabaseModel>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
    DatabaseModel& db = *iter;
    if (db.oid == incoming.oid) {
      db.MergeContents(incoming);
      return;
    }
  }
  wxASSERT(false);
}

void ServerModel::UpdateRoles(const std::vector<RoleModel>& incoming)
{
  roles.clear();
  roles.reserve(incoming.size());
  std::copy(incoming.begin(), incoming.end(), std::back_inserter(roles));
}

void ServerModel::UpdateTablespaces(const std::vector<TablespaceModel>& incoming)
{
  tablespaces.clear();
  tablespaces.reserve(incoming.size());
  std::copy(incoming.begin(), incoming.end(), std::back_inserter(tablespaces));
}

void DatabaseModel::MergeContents(const DatabaseModel& incoming)
{
  loaded = true;
  catalogueIndex = NULL;
  schemas = incoming.schemas;
  extensions = incoming.extensions;
  relations = incoming.relations;
  functions = incoming.functions;
  textSearchDictionaries = incoming.textSearchDictionaries;
  textSearchParsers = incoming.textSearchParsers;
  textSearchTemplates = incoming.textSearchTemplates;
  textSearchConfigurations = incoming.textSearchConfigurations;
  types = incoming.types;
  operators = incoming.operators;
}

ObjectModel *DatabaseModel::FindObject(const ObjectModelReference& ref)
{
  return const_cast<ObjectModel*>( ((const DatabaseModel*) this )->FindObject(ref) );
}

const ObjectModel *DatabaseModel::FindObject(const ObjectModelReference& ref) const
{
  switch (ref.GetObjectClass()) {
  case ObjectModelReference::PG_CLASS:
    return SearchContents(relations, ref.GetOid());
  case ObjectModelReference::PG_PROC:
    return SearchContents(functions, ref.GetOid());
  case ObjectModelReference::PG_TS_CONFIG:
    return SearchContents(textSearchConfigurations, ref.GetOid());
  case ObjectModelReference::PG_TS_DICT:
    return SearchContents(textSearchDictionaries, ref.GetOid());
  case ObjectModelReference::PG_TS_PARSER:
    return SearchContents(textSearchParsers, ref.GetOid());
  case ObjectModelReference::PG_TS_TEMPLATE:
    return SearchContents(textSearchTemplates, ref.GetOid());
  case ObjectModelReference::PG_TYPE:
    return SearchContents(types, ref.GetOid());
  case ObjectModelReference::PG_OPERATOR:
    return SearchContents(operators, ref.GetOid());
  case ObjectModelReference::PG_NAMESPACE:
    return SearchContents(schemas, ref.GetOid());
  case ObjectModelReference::PG_EXTENSION:
    return SearchContents(extensions, ref.GetOid());
  default:
    return NULL;
  }
}

DatabaseModel::Divisions DatabaseModel::DivideSchemaMembers() const
{
  std::vector<const SchemaMemberModel*> members;

  std::transform(relations.begin(), relations.end(), std::back_inserter(members), TakePointer);
  std::transform(functions.begin(), functions.end(), std::back_inserter(members), TakePointer);
  std::transform(textSearchDictionaries.begin(), textSearchDictionaries.end(), std::back_inserter(members), TakePointer);
  std::transform(textSearchParsers.begin(), textSearchParsers.end(), std::back_inserter(members), TakePointer);
  std::transform(textSearchTemplates.begin(), textSearchTemplates.end(), std::back_inserter(members), TakePointer);
  std::transform(textSearchConfigurations.begin(), textSearchConfigurations.end(), std::back_inserter(members), TakePointer);
  std::transform(types.begin(), types.end(), std::back_inserter(members), TakePointer);
  std::transform(operators.begin(), operators.end(), std::back_inserter(members), TakePointer);

  std::sort(members.begin(), members.end(), SchemaMemberModel::CollateByQualifiedName);

  Divisions result;

  // in case there are any empty extensions...
  for (std::vector<ExtensionModel>::const_iterator iter = extensions.begin(); iter != extensions.end(); iter++) {
    result.extensionDivisions[*iter];
  }

  for (std::vector<const SchemaMemberModel*>::iterator iter = members.begin(); iter != members.end(); iter++) {
    const SchemaMemberModel *member = *iter;
    if (member->extension.oid != InvalidOid)
      result.extensionDivisions[member->extension].push_back(member);
    else if (!member->IsUser())
      result.systemDivision.push_back(member);
    else
      result.userDivision.push_back(member);
  }

  return result;
}

void DatabaseModel::SubmitWork(ObjectBrowserManagedWork *work)
{
  wxGetApp().GetObjectBrowserModel().SubmitDatabaseWork(this, work);
}

void DatabaseModel::Load(IndexSchemaCompletionCallback *indexCompletion)
{
  SubmitWork(new LoadDatabaseWork(*this, indexCompletion == NULL));
  SubmitWork(new IndexDatabaseSchemaWork(*this, indexCompletion));
  SubmitWork(new LoadDatabaseDescriptionsWork(*this));
}

void DatabaseModel::LoadRelation(const ObjectModelReference& relationRef)
{
  RelationModel *relation = FindRelation(relationRef);
  wxASSERT(relation != NULL);
  SubmitWork(new LoadRelationWork(relation->type, relationRef));
}

DatabaseModel::~DatabaseModel()
{
  if (catalogueIndex != NULL)
    delete catalogueIndex;
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
