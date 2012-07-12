/**
 * @file
 * Database interaction performed by the object browser
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_database_work_impl_h
#define __object_browser_database_work_impl_h

#include "object_browser_database_work.h"

/**
 * Initialise database connection.
 *
 * Run once on each database connection added to the object browser.
 */
class SetupDatabaseConnectionWork : public ObjectBrowserWork {
public:
  SetupDatabaseConnectionWork(const ObjectModelReference& databaseRef) : ObjectBrowserWork(databaseRef) {}
protected:
  void operator()()
  {
    owner->DoCommand(_T("SetupObjectBrowserConnection"));
  }
  void UpdateModel(ObjectBrowserModel *model)
  {
  }
  void UpdateView(ObjectBrowser *ob)
  {
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
   */
  RefreshDatabaseListWork(const ObjectModelReference& database) : ObjectBrowserWork(database), serverId(database.GetServerId())
  {
    wxLogDebug(_T("%p: work to load database list"), this);
  }
protected:
  void operator()();
  void UpdateModel(ObjectBrowserModel *model);
  void UpdateView(ObjectBrowser *ob);
private:
  const wxString serverId;
  wxString serverVersionString;
  int serverVersion;
  SSL *ssl;
  std::vector<DatabaseModel> databases;
  std::vector<RoleModel> roles;
  std::vector<TablespaceModel> tablespaces;
  void ReadServer();
  void ReadDatabases();
  void ReadRoles();
  void ReadTablespaces();
};

/**
 * Load schema members (relations and functions) for a database.
 */
class LoadDatabaseSchemaWork : public ObjectBrowserWork {
public:
  /**
   * Create work object.
   * @param databaseModel Database model to populate
   * @param expandAfter Expand tree item after populating
   */
  LoadDatabaseSchemaWork(const ObjectModelReference& databaseRef, bool expandAfter) : ObjectBrowserWork(databaseRef), databaseRef(databaseRef), expandAfter(expandAfter) {
    wxLogDebug(_T("%p: work to load schema"), this);
  }
private:
  const ObjectModelReference databaseRef;
  const bool expandAfter;
  DatabaseModel incoming;
protected:
  void operator()();
  void LoadRelations();
  void LoadFunctions();
  template<typename T>
  void LoadSimpleSchemaMembers(const wxString &queryName, typename std::vector<T>& vec);
  void UpdateModel(ObjectBrowserModel *model);
  void UpdateView(ObjectBrowser *ob);
private:
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
  LoadDatabaseDescriptionsWork(const ObjectModelReference& databaseRef) : ObjectBrowserWork(databaseRef), databaseRef(databaseRef) {
    wxLogDebug(_T("%p: work to load schema object descriptions"), this);
  }
private:
  const ObjectModelReference databaseRef;
  std::map<unsigned long, wxString> descriptions;
protected:
  void operator()();
  void UpdateModel(ObjectBrowserModel *model);
  void UpdateView(ObjectBrowser *ob);
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
  IndexDatabaseSchemaWork(const ObjectModelReference& databaseRef, IndexSchemaCompletionCallback *completion = NULL) : ObjectBrowserWork(databaseRef), databaseRef(databaseRef), completion(completion) {
    wxLogDebug(_T("%p: work to index schema"), this);
  }
private:
  const ObjectModelReference databaseRef;
  IndexSchemaCompletionCallback *completion;
  CatalogueIndex *catalogueIndex;
  static const std::map<wxString, CatalogueIndex::Type> typeMap;
protected:
  void operator()();
  void UpdateModel(ObjectBrowserModel *model);
  void UpdateView(ObjectBrowser *ob);
};

/**
 * Load relation details from database
 */
class LoadRelationWork : public ObjectBrowserWork {
public:
  /**
   * @param relationModel Relation model to populate
   */
  LoadRelationWork(RelationModel::Type relationType, const ObjectModelReference& relationRef) : ObjectBrowserWork(relationRef.DatabaseRef()), relationType(relationType), relationRef(relationRef) {
    wxLogDebug(_T("%p: work to load relation"), this);
  }
private:
  const RelationModel::Type relationType;
  const ObjectModelReference relationRef;
  RelationModel incoming;
private:
  void operator()();
  void ReadColumns();
  void ReadIndices();
  void ReadTriggers();
  void ReadSequences();
  void ReadConstraints();
  void UpdateModel(ObjectBrowserModel *model);
  void UpdateView(ObjectBrowser *ob);
  static std::vector<int> ParseInt2Vector(const wxString &str);
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
