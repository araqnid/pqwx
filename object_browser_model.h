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
  unsigned long oid;
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
  std::map<unsigned long,wxTreeItemId> symbolItemLookup;
  std::vector<RelationModel*> relations;
  std::vector<FunctionModel*> functions;

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
  std::vector<DatabaseModel*> databases;
  int serverVersion;
  bool usingSSL;
  std::map<wxString, DatabaseConnection*> connections;
  void SetVersion(int version) {
    serverVersion = version;
  }
  void Dispose();
  void BeginDisconnectAll(std::vector<DatabaseConnection*> &disconnecting);
};

static inline bool emptySchema(std::vector<RelationModel*> schemaRelations) {
  return schemaRelations.size() == 1 && schemaRelations[0]->name.IsSameAs(_T(""));
}

#endif
