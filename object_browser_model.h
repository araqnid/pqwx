// -*- mode: c++ -*-

#ifndef __object_browser_model_h
#define __object_browser_model_h

class ObjectModel : public wxTreeItemData {
public:
  wxString name;
  wxString description;
};

class ColumnModel : public ObjectModel {
public:
  RelationModel *relation;
  wxString type;
  bool nullable;
  bool hasDefault;
};

class IndexModel : public ObjectModel {
public:
  RelationModel *relation;
  vector<wxString> columns;
  wxString type;
};

class TriggerModel : public ObjectModel {
public:
  RelationModel *relation;
};

class SchemaMemberModel : public ObjectModel {
public:
  DatabaseModel *database;
  unsigned long oid;
  wxString schema;
  bool user;
};

class RelationModel : public SchemaMemberModel {
public:
  enum Type { TABLE, VIEW, SEQUENCE } type;
};

class FunctionModel : public SchemaMemberModel {
public:
  wxString prototype;
  enum Type { SCALAR, RECORDSET, TRIGGER, AGGREGATE, WINDOW } type;
};

class DatabaseModel : public ObjectModel {
public:
  unsigned long oid;
  bool isTemplate;
  bool allowConnections;
  bool havePrivsToConnect;
  bool loaded;
  ServerModel *server;
  CatalogueIndex *catalogueIndex;
  bool IsUsable() {
    return allowConnections && havePrivsToConnect;
  }
  bool IsSystem() {
    return name.IsSameAs(_T("postgres")) || name.IsSameAs(_T("template0")) || name.IsSameAs(_T("template1"));
  }
  map<unsigned long,wxTreeItemId> symbolItemLookup;
};

class RoleModel : public ObjectModel {
public:
  unsigned long oid;
  bool superuser;
  bool canLogin;
};

class ServerModel : public wxTreeItemData {
public:
  ServerModel() {}
  ServerModel(DatabaseConnection *db) {
    connections[db->DbName()] = db;
    db->Relabel(_("Object Browser"));
  }
  ServerConnection *conn;
  vector<DatabaseModel*> databases;
  int serverVersion;
  bool usingSSL;
  map<wxString, DatabaseConnection*> connections;
  void SetVersion(int version) {
    serverVersion = version;
  }
  DatabaseModel *findDatabase(unsigned long oid) {
    for (vector<DatabaseModel*>::iterator iter = databases.begin(); iter != databases.end(); iter++) {
      if ((*iter)->oid == oid)
	return *iter;
    }
    return NULL;
  }
  bool versionNotBefore(int major, int minor) {
    return serverVersion >= (major * 10000 + minor * 100);
  }
  void Dispose() {
    wxLogDebug(_T("Disposing of server %s"), conn->Identification().c_str());
    for (map<wxString, DatabaseConnection*>::iterator iter = connections.begin(); iter != connections.end(); iter++) {
      DatabaseConnection *db = iter->second;
      wxLogDebug(_T(" Closing connection to %s"), iter->first.c_str());
      db->CloseSync();
    }
    connections.clear();
  }
};

static inline bool emptySchema(vector<RelationModel*> schemaRelations) {
  return schemaRelations.size() == 1 && schemaRelations[0]->name.IsSameAs(_T(""));
}

#endif
