/**
 * @file
 * A reference to a server-side object
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __object_model_reference_h
#define __object_model_reference_h

#include "libpq-fe.h"

/**
 * A "soft" reference into the object model.
 */
class ObjectModelReference {
public:
  ObjectModelReference(const wxString& serverId) : serverId(serverId), database(InvalidOid), regclass(InvalidOid), oid(InvalidOid), subid(0) {}
  ObjectModelReference(const wxString& serverId, Oid database) : serverId(serverId), database(database), regclass(PG_DATABASE), oid(database), subid(0) {}
  ObjectModelReference(const wxString& serverId, Oid regclass, Oid oid) : serverId(serverId), database(InvalidOid), regclass(regclass), oid(oid), subid(0) {}
  ObjectModelReference(const ObjectModelReference& database, Oid regclass, Oid oid, int subid = 0) : serverId(database.serverId), database(database.oid), regclass(regclass), oid(oid), subid(subid) {}

  wxString Identify() const;

  wxString GetServerId() const { return serverId; }
  Oid GetDatabase() const { return database; }
  Oid GetObjectClass() const { return regclass; }
  Oid GetOid() const { return oid; }
  int GetObjectSubid() const { return subid; }

  ObjectModelReference ServerRef() const { return ObjectModelReference(serverId); }
  ObjectModelReference DatabaseRef() const { return ObjectModelReference(serverId, database); }

  bool operator<(const ObjectModelReference& other) const
  {
    if (serverId < other.serverId) return true;
    else if (serverId > other.serverId) return false;
    if (regclass < other.regclass) return true;
    else if (regclass > other.regclass) return false;
    if (database < other.database) return true;
    else if (database > other.database) return false;
    if (oid < other.oid) return true;
    else if (oid > other.oid) return false;
    return subid < other.subid;
  }

  bool operator==(const ObjectModelReference& other) const
  {
    if (&other == this) return true;
    if (serverId != other.serverId) return false;
    if (database != other.database) return false;
    if (regclass != other.regclass) return false;
    if (oid != other.oid) return false;
    if (subid != other.subid) return false;
    return true;
  }

  bool operator!=(const ObjectModelReference& other) const
  {
    if (&other == this) return false;
    if (serverId != other.serverId) return true;
    if (database != other.database) return true;
    if (regclass != other.regclass) return true;
    if (oid != other.oid) return true;
    if (subid != other.subid) return true;
    return false;
  }

  static const Oid PG_ATTRIBUTE = 1249;
  static const Oid PG_CLASS = 1259;
  static const Oid PG_CONSTRAINT = 2606;
  static const Oid PG_DATABASE = 1262;
  static const Oid PG_EXTENSION = 3079;
  static const Oid PG_INDEX = 2610;
  static const Oid PG_NAMESPACE = 2615;
  static const Oid PG_PROC = 1255;
  static const Oid PG_ROLE = 1260;
  static const Oid PG_TABLESPACE = 1213;
  static const Oid PG_TRIGGER = 2620;
  static const Oid PG_TS_CONFIG = 3602;
  static const Oid PG_TS_DICT = 3600;
  static const Oid PG_TS_PARSER = 3601;
  static const Oid PG_TS_TEMPLATE = 3764;
private:
  wxString serverId;
  Oid database;
  Oid regclass;
  Oid oid;
  int subid;
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
