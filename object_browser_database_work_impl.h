/**
 * @file
 * Database interaction performed by the object browser
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_browser_database_work_impl_h
#define __object_browser_database_work_impl_h

#include "object_browser_work.h"
#include "object_browser_model.h"

/**
 * Initialise database connection.
 *
 * Run once on each database connection added to the object browser.
 */
class SetupDatabaseConnectionWork : public ObjectBrowserWork {
public:
  SetupDatabaseConnectionWork(const ObjectModelReference& databaseRef) : ObjectBrowserWork(databaseRef) {}
protected:
  void DoManagedWork()
  {
    owner->DoCommand(_T("SetupObjectBrowserConnection"));
  }
  void UpdateModel(ObjectBrowserModel& model)
  {
  }
  void UpdateView(ObjectBrowser& ob)
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
  void DoManagedWork();
  void UpdateModel(ObjectBrowserModel& model);
  void UpdateView(ObjectBrowser& ob);
private:
  const wxString serverId;
  wxString serverVersionString;
  int serverVersion;
  SSLInfo *sslInfo;
  bool createDB;
  bool createUser;
  bool superuser;
  wxString rolename;
  std::vector<DatabaseModel> databases;
  std::vector<RoleModel> roles;
  std::vector<TablespaceModel> tablespaces;
  void ReadServer();
  void ReadRole();
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
  void DoManagedWork();
  void UpdateModel(ObjectBrowserModel& model);
  void UpdateView(ObjectBrowser& ob);
private:
  static SchemaModel ReadSchema(const QueryResults::Row&);
  static ExtensionModel ReadExtension(const QueryResults::Row&);
  template<typename T>
  class Mapper {
  public:
    Mapper(const LoadDatabaseSchemaWork& owner) : owner(owner) {}
    T operator()(const QueryResults::Row&);
  protected:
    SchemaModel Schema(Oid oid) { return owner.Schema(oid); }
    ExtensionModel Extension(Oid oid) { return owner.Extension(oid); }
  private:
    const LoadDatabaseSchemaWork& owner;
  };
  std::map<Oid, SchemaModel> schemas;
  std::map<Oid, ExtensionModel> extensions;
  SchemaModel Schema(Oid oid) const { return InternalLookup(schemas, oid); }
  ExtensionModel Extension(Oid oid) const { return oid == InvalidOid ? ExtensionModel() : InternalLookup(extensions, oid); }
  template<class T>
  const T& InternalLookup(const typename std::map<Oid, T>& table, Oid oid) const;
  template<class T, class InputIterator>
  void PopulateInternalLookup(typename std::map<Oid, T>& table, InputIterator first, InputIterator last);
  template<class OutputIterator, class UnaryOperator>
  void LoadThings(const wxString& queryName, OutputIterator output, UnaryOperator mapper);
  template<class Container>
  void LoadThings(const wxString& queryName, Container& target)
  {
    LoadThings(queryName, std::back_inserter(target), Mapper<typename Container::value_type>(*this));
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
  LoadDatabaseDescriptionsWork(const ObjectModelReference& databaseRef) : ObjectBrowserWork(databaseRef), databaseRef(databaseRef) {
    wxLogDebug(_T("%p: work to load schema object descriptions"), this);
  }
private:
  const ObjectModelReference databaseRef;
  std::map<unsigned long, wxString> descriptions;
protected:
  void DoManagedWork();
  void UpdateModel(ObjectBrowserModel& model);
  void UpdateView(ObjectBrowser& ob);
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
  IndexDatabaseSchemaWork(const ObjectModelReference& databaseRef) : ObjectBrowserWork(databaseRef), databaseRef(databaseRef) {
    wxLogDebug(_T("%p: work to index schema"), this);
  }

  IndexDatabaseSchemaWork(const ObjectModelReference& databaseRef, IndexSchemaCompletionCallback *indexCompletion) : ObjectBrowserWork(databaseRef, new CallCompletion(this, indexCompletion)), databaseRef(databaseRef) {
    wxLogDebug(_T("%p: work to index schema"), this);
  }
private:
  const ObjectModelReference databaseRef;
  CatalogueIndex *catalogueIndex;
  static const std::map<wxString, CatalogueIndex::Type> typeMap;
  class CallCompletion : public CompletionCallback {
  public:
    CallCompletion(IndexDatabaseSchemaWork *owner, IndexSchemaCompletionCallback *indexCompletion) : owner(owner), indexCompletion(indexCompletion) {}
    void OnCompletion()
    {
      if (indexCompletion != NULL) {
        indexCompletion->Completed(*(owner->catalogueIndex));
        delete indexCompletion;
      }
    }
    void OnCrash()
    {
      if (indexCompletion != NULL) {
        delete indexCompletion;
      }
    }
  private:
    IndexDatabaseSchemaWork * const owner;
    IndexSchemaCompletionCallback * const indexCompletion;
  };
protected:
  void DoManagedWork();
  void UpdateModel(ObjectBrowserModel& model);
  void UpdateView(ObjectBrowser& ob) {}
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
  void DoManagedWork();
  void ReadColumns();
  void ReadIndices();
  void ReadTriggers();
  void ReadSequences();
  void ReadConstraints();
  void UpdateModel(ObjectBrowserModel& model);
  void UpdateView(ObjectBrowser& ob);
  static std::vector<int> ParseInt2Vector(const wxString &str);
};

/**
 * Drop database.
 */
class DropDatabaseWork : public ObjectBrowserWork {
public:
  DropDatabaseWork(const ObjectModelReference& ref, const wxString& dbname) : ObjectBrowserWork(ref, NULL, NO_TRANSACTION), serverId(ref.GetServerId()), dbname(dbname)
  {
    wxLogDebug(_T("%p: work to drop database"), this);
  }

  void DoManagedWork();
  void UpdateModel(ObjectBrowserModel&);
  void UpdateView(ObjectBrowser&);

private:
  const wxString serverId;
  const wxString dbname;
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
