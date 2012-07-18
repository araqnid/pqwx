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
class LoadServerWork : public ObjectBrowserWork {
public:
  /**
   * Create work object
   * @param serverModel Server model to populate
   */
  LoadServerWork(const ObjectModelReference& database) : ObjectBrowserWork(database), serverId(database.GetServerId())
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
  template<class Container, class UnaryOperator>
  void LoadThings(const wxString& queryName, Container output, UnaryOperator mapper)
  {
    QueryResults rs = Query(queryName).List();
    std::transform(rs.Rows().begin(), rs.Rows().end(), std::back_inserter(output), mapper);
  }
  template<class T, class UnaryOperator>
  void LoadThings(const wxString& queryName, std::vector<T>& output, UnaryOperator mapper)
  {
    QueryResults rs = Query(queryName).List();
    output.reserve(rs.Rows().size());
    std::transform(rs.Rows().begin(), rs.Rows().end(), std::back_inserter(output), mapper);
  }
  void ReadServer();
  void ReadCurrentRole();
  static DatabaseModel ReadDatabase(const QueryResults::Row&);
  static RoleModel ReadRole(const QueryResults::Row&);
  static TablespaceModel ReadTablespace(const QueryResults::Row&);
};

/**
 * Load schema members (relations and functions) for a database.
 */
class LoadDatabaseWork : public ObjectBrowserWork {
public:
  /**
   * Create work object.
   * @param databaseModel Database model to populate
   * @param expandAfter Expand tree item after populating
   */
  LoadDatabaseWork(const ObjectModelReference& databaseRef, bool expandAfter) : ObjectBrowserWork(databaseRef), databaseRef(databaseRef), expandAfter(expandAfter) {
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
    Mapper(const LoadDatabaseWork& owner) : owner(owner) {}
    T operator()(const QueryResults::Row&);
  protected:
    SchemaModel Schema(Oid oid) { return owner.Schema(oid); }
    ExtensionModel Extension(Oid oid) { return owner.Extension(oid); }
  private:
    const LoadDatabaseWork& owner;
  };
  std::map<Oid, SchemaModel> schemas;
  std::map<Oid, ExtensionModel> extensions;
  SchemaModel Schema(Oid oid) const { return InternalLookup(schemas, oid); }
  ExtensionModel Extension(Oid oid) const { return oid == InvalidOid ? ExtensionModel() : InternalLookup(extensions, oid); }
  template<class T>
  const T& InternalLookup(const typename std::map<Oid, T>& table, Oid oid) const;
  template<class T, class InputIterator>
  void PopulateInternalLookup(typename std::map<Oid, T>& table, InputIterator first, InputIterator last);
  template<class T, class UnaryOperator>
  void LoadThings(const wxString& queryName, std::vector<T>& target, UnaryOperator mapper)
  {
    QueryResults rs = Query(queryName).List();
    target.reserve(rs.Rows().size());
    std::transform(rs.Rows().begin(), rs.Rows().end(), std::back_inserter(target), mapper);
  }
  template<class T>
  void LoadThings(const wxString& queryName, std::vector<T>& target)
  {
    QueryResults rs = Query(queryName).List();
    target.reserve(rs.Rows().size());
    std::transform(rs.Rows().begin(), rs.Rows().end(), std::back_inserter(target), Mapper<T>(*this));
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
  std::map<Oid, wxString> descriptions;
  template<typename InputIterator>
  unsigned PutDescriptions(InputIterator iter, InputIterator last) const
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
  static ColumnModel ReadColumn(const QueryResults::Row&);
  void LoadIndices();
  static TriggerModel ReadTrigger(const QueryResults::Row&);
  static RelationModel ReadSequence(const QueryResults::Row&);
  class LoadConstraint {
  public:
    LoadConstraint(LoadRelationWork& parent) : checkConstraints(parent.incoming.checkConstraints) {}
    void operator()(const QueryResults::Row&);
  private:
    std::vector<CheckConstraintModel>& checkConstraints;
  };
  void UpdateModel(ObjectBrowserModel& model);
  void UpdateView(ObjectBrowser& ob);
  static std::vector<int> ParseInt2Vector(const wxString &str);
  template<class UnaryOperator>
  void LoadThings(const wxString& queryName, UnaryOperator loader)
  {
    QueryResults rs = Query(queryName).OidParam(relationRef.GetOid()).List();
    std::for_each(rs.Rows().begin(), rs.Rows().end(), loader);
  }
  template<class T, class UnaryOperator>
  void LoadThings(const wxString& queryName, std::vector<T>& target, UnaryOperator mapper)
  {
    QueryResults rs = Query(queryName).OidParam(relationRef.GetOid()).List();
    target.reserve(rs.Rows().size());
    std::transform(rs.Rows().begin(), rs.Rows().end(), std::back_inserter(target), mapper);
  }
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
