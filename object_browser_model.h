// -*- mode: c++ -*-

#ifndef __object_browser_model_h
#define __object_browser_model_h

#include "database_work.h"

class ObjectModel : public wxTreeItemData {
public:
  wxString name;
  wxString description;
  virtual wxString FormatName() const { return name; }
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
  std::vector<wxString> columns;
  wxString type;
};

class TriggerModel : public ObjectModel {
public:
  RelationModel *relation;
};

class SchemaMemberModel : public ObjectModel {
public:
  DatabaseModel *database;
  Oid oid;
  wxString schema;
  wxString extension;
  bool user;
  wxString FormatName() const { return schema + _T('.') + name; }
};

class RelationModel : public SchemaMemberModel {
public:
  enum Type { TABLE, VIEW, SEQUENCE } type;
};

class FunctionModel : public SchemaMemberModel {
public:
  wxString arguments;
  enum Type { SCALAR, RECORDSET, TRIGGER, AGGREGATE, WINDOW } type;
  wxString FormatName() const { return schema + _T('.') + name + _T('(') + arguments + _T(')'); }
};

class DatabaseModel : public ObjectModel {
public:
  DatabaseModel() : catalogueIndex(NULL){}
  virtual ~DatabaseModel() {
    if (catalogueIndex != NULL)
      delete catalogueIndex;
  }
  Oid oid;
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
    return name == _T("postgres") || name == _T("template0") || name == _T("template1");
  }
  std::map<unsigned long,wxTreeItemId> symbolItemLookup;
  std::vector<RelationModel*> relations;
  std::vector<FunctionModel*> functions;

  wxString Identification() const;
  DatabaseConnection *GetDatabaseConnection();

  class Divisions {
  public:
    std::vector<SchemaMemberModel*> userDivision;
    std::vector<SchemaMemberModel*> systemDivision;
    std::map<wxString, std::vector<SchemaMemberModel*> > extensionDivisions;
  };

  Divisions DivideSchemaMembers() const {
    std::vector<SchemaMemberModel*> members;
    for (std::vector<RelationModel*>::const_iterator iter = relations.begin(); iter != relations.end(); iter++) {
      members.push_back(*iter);
    }
    for (std::vector<FunctionModel*>::const_iterator iter = functions.begin(); iter != functions.end(); iter++) {
      members.push_back(*iter);
    }

    sort(members.begin(), members.end(), CollateSchemaMembers);
  
    Divisions result;

    for (std::vector<SchemaMemberModel*>::iterator iter = members.begin(); iter != members.end(); iter++) {
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
    if (r1->schema == r2->schema) {
      return r1->name < r2->name;
    }
    return false;
  }

};

class RoleModel : public ObjectModel {
public:
  Oid oid;
  bool superuser;
  bool canLogin;
};

class ServerModel : public wxTreeItemData {
public:
  ServerModel(ServerConnection *conn) : conn(conn) {}
  ServerModel(ServerConnection *conn, DatabaseConnection *db) : conn(conn) {
    connections[db->DbName()] = db;
    db->Relabel(_("Object Browser"));
  }
  void ReadServerParameters(const char *serverVersionRaw, int serverVersion_, void *ssl) {
    serverVersion = serverVersion_;
    serverVersionString = wxString(serverVersionRaw, wxConvUTF8);
    usingSSL = (ssl != NULL);
  }
  void Dispose();
  void BeginDisconnectAll(std::vector<DatabaseConnection*> &disconnecting);
  const wxString& Identification() const { return conn->Identification(); }
  const wxString& GlobalDbName() const { return conn->globalDbName; }
  const wxString& VersionString() const { return serverVersionString; }
  bool IsUsingSSL() const { return usingSSL; }
  DatabaseConnection *GetDatabaseConnection(const wxString &dbname);
  DatabaseConnection *GetServerAdminConnection() { return GetDatabaseConnection(GlobalDbName()); }
  //const ServerConnection* ConnectionProperties() const { return conn; }
private:
  const ServerConnection *conn;
  std::vector<DatabaseModel*> databases;
  int serverVersion;
  wxString serverVersionString;
  bool usingSSL;
  std::map<wxString, DatabaseConnection*> connections;
};

static inline bool emptySchema(std::vector<RelationModel*> schemaRelations) {
  return schemaRelations.size() == 1 && schemaRelations[0]->name.IsEmpty();
}

inline wxString DatabaseModel::Identification() const {
  return server->Identification() + _T(' ') + name;
}

inline DatabaseConnection *DatabaseModel::GetDatabaseConnection() {
  return server->GetDatabaseConnection(name);
}

#endif
