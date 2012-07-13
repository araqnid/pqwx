#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"

void SequenceScriptWork::GenerateScript(OutputIterator output)
{
  wxString sqlTemplate = wxString(owner->GetSql(_T("Sequence Detail")), wxConvUTF8);
  wxString sequenceName = (owner->Query(_T("Sequence Name")).OidParam(reloid).UniqueResult())[0];
  sqlTemplate.Replace(_T("$sequence"), sequenceName, true);
  QueryResults::Row sequenceDetail = owner->DatabaseWork::Query(sqlTemplate.utf8_str()).OidParam(reloid).UniqueResult();

  switch (mode) {
  case Create: {
    wxString sql;
    sql << _T("CREATE SEQUENCE ") << sequenceName;
    sql << _T("\n\tINCREMENT BY ") << sequenceDetail[2];
    sql << _T("\n\tMINVALUE ") << sequenceDetail[3];
    sql << _T("\n\tMAXVALUE ") << sequenceDetail[4];
    sql << _T("\n\tSTART WITH ") << sequenceDetail[5];
    sql << _T("\n\tCACHE ") << sequenceDetail[6];
    bool cycled = sequenceDetail.ReadBool(7);
    sql << ( cycled ? _T("\n\tNO CYCLE") : _T("\n\tCYCLE") );
    sql << _T("\n\tOWNED BY ") << (sequenceDetail[8].empty() ? _T("NONE") : sequenceDetail[8]);

    *output++ = sql;

    *output++ = _T("ALTER SEQUENCE ") + sequenceName + _T(" OWNER TO ") + sequenceDetail[0];

    PgAcl(sequenceDetail[1]).GenerateGrantStatements(output, sequenceDetail[0], _T("SEQUENCE ") + sequenceName, privilegeMap);

    AddDescription(output, _T("SEQUENCE"), sequenceName, sequenceDetail[9]);
  }
    break;

  case Alter: {
    wxString sql;

    sql << _T("ALTER SEQUENCE ") << sequenceName;
    sql << _T("\n\tINCREMENT BY ") << sequenceDetail[2];
    sql << _T("\n\tMINVALUE ") << sequenceDetail[3];
    sql << _T("\n\tMAXVALUE ") << sequenceDetail[4];
    sql << _T("\n\tRESTART WITH ") << sequenceDetail[5];
    sql << _T("\n\tCACHE ") << sequenceDetail[6];
    bool cycled = sequenceDetail.ReadBool(7);
    sql << ( cycled ? _T("\n\tNO CYCLE") : _T("\n\tCYCLE") );
    sql << _T("\n\tOWNED BY ") << (sequenceDetail[8].empty() ? _T("NONE") : sequenceDetail[8]);

    *output++ = sql;

    *output++ = _T("ALTER SEQUENCE ") + sequenceName + _T(" OWNER TO ") + sequenceDetail[0];

    PgAcl(sequenceDetail[1]).GenerateGrantStatements(output, sequenceDetail[0], _T("SEQUENCE ") + sequenceName, privilegeMap);

    AddDescription(output, _T("SEQUENCE"), sequenceName, sequenceDetail[9]);
  }
    break;

  case Drop:
    *output++ = _T("DROP SEQUENCE ") + sequenceName;
    break;

  default:
    wxASSERT(false);
  }
}

std::map<wxChar, wxString> SequenceScriptWork::privilegeMap = PrivilegeMap(_T("r=SELECT w=UPDATE U=USAGE"));

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
