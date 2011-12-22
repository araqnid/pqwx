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
  wxString extension;
  bool user;
};

class RelationModel : public SchemaMemberModel {
public:
  enum Type { TABLE, VIEW, SEQUENCE } type;
};

class FunctionModel : public SchemaMemberModel {
public:
  wxString arguments;
  enum Type { SCALAR, RECORDSET, TRIGGER, AGGREGATE, WINDOW } type;
};

class DatabaseModel : public ObjectModel {
public:
  DatabaseModel() : catalogueIndex(NULL){}
  virtual ~DatabaseModel() {
    if (catalogueIndex != NULL)
      delete catalogueIndex;
  }
  unsigned long oid;
  bool isTemplate;
  bool allowConnections;
  bool havePrivsToConnect;
  bool loaded;
  ServerModel *server;
  const CatalogueIndex *catalogueIndex;
  bool IsUsable() {
    return allowConnections && havePrivsToConnect;
  }
  bool IsSystem() {
    return name.IsSameAs(_T("postgres")) || name.IsSameAs(_T("template0")) || name.IsSameAs(_T("template1"));
  }
  map<unsigned long,wxTreeItemId> symbolItemLookup;
  vector<RelationModel*> relations;
  vector<FunctionModel*> functions;

  class Divisions {
  public:
    vector<SchemaMemberModel*> userDivision;
    vector<SchemaMemberModel*> systemDivision;
    map<wxString, vector<SchemaMemberModel*> > extensionDivisions;
  };

  Divisions DivideSchemaMembers() const {
    vector<SchemaMemberModel*> members;
    for (vector<RelationModel*>::const_iterator iter = relations.begin(); iter != relations.end(); iter++) {
      members.push_back(*iter);
    }
    for (vector<FunctionModel*>::const_iterator iter = functions.begin(); iter != functions.end(); iter++) {
      members.push_back(*iter);
    }

    sort(members.begin(), members.end(), CollateSchemaMembers);
  
    Divisions result;

    for (vector<SchemaMemberModel*>::iterator iter = members.begin(); iter != members.end(); iter++) {
      SchemaMemberModel *member = *iter;
      if (!member->extension.IsEmpty())
	result.extensionDivisions[member->extension].push_back(member);
      else if (!member->user)
	result.systemDivision.push_back(member);
      else
	result.userDivision.push_back(member);
    }

    return result;
  }

private:
  static bool CollateSchemaMembers(SchemaMemberModel *r1, SchemaMemberModel *r2) {
    if (r1->schema < r2->schema) return true;
    if (r1->schema.IsSameAs(r2->schema)) {
      return r1->name < r2->name;
    }
    return false;
  }

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
  const ServerConnection *conn;
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
      wxLogDebug(_T(" Sending disconnect request to %s"), iter->first.c_str());
      db->AddWork(new DisconnectWork());
    }
    for (map<wxString, DatabaseConnection*>::iterator iter = connections.begin(); iter != connections.end(); iter++) {
      DatabaseConnection *db = iter->second;
      wxLogDebug(_T(" Waiting for connection to %s to exit"), iter->first.c_str());
      db->WaitUntilClosed();
    }
    connections.clear();
  }
};

static inline bool emptySchema(vector<RelationModel*> schemaRelations) {
  return schemaRelations.size() == 1 && schemaRelations[0]->name.IsSameAs(_T(""));
}

#endif
