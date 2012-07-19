#include "wx/wxprec.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "object_browser_scripts.h"
#include "object_browser_scripts_util.h"

void OperatorScriptWork::GenerateScript(OutputIterator output)
{
  QueryResults::Row detail = Query(_T("Operator Detail")).OidParam(oproid).UniqueResult();
  wxString name = detail[0];
  wxString schemaName = QuoteIdent(detail[1]);
  wxString owner = QuoteIdent(detail[2]);
  wxString qualifiedName = schemaName + _T('.') + name;

  switch (mode) {
  case Create: {
    wxString kind = detail.ReadText(3);
    bool canMerge = detail.ReadBool(4);
    bool canHash = detail.ReadBool(5);
    wxString leftTypeName = detail.ReadText(6);
    wxString rightTypeName = detail.ReadText(7);
    wxString resultTypeName = detail.ReadText(8);
    wxString commutatorOper = detail.ReadText(9);
    wxString negatorOper = detail.ReadText(10);
    wxString implProc = detail.ReadText(11);
    wxString restrictProc = detail.ReadText(12);
    wxString joinProc = detail.ReadText(13);
    wxString description = detail.ReadText(14);
    wxString sql;
    sql << _T("CREATE OPERATOR ") << qualifiedName << _T("(\n")
        << _T("\tPROCEDURE = ") << implProc;
    if (kind == _T("b") || kind == _T("l"))
      sql << _T(",\n\tLEFTARG = ") << leftTypeName;
    if (kind == _T("b") || kind == _T("r"))
      sql << _T(",\n\tRIGHTARG = ") << rightTypeName;
    if (commutatorOper != _T("0"))
      sql << _T(",\n\tCOMMUTATOR = operator(") << commutatorOper << _T(")");
    if (negatorOper != _T("0"))
      sql << _T(",\n\tNEGATOR = operator(") << negatorOper << _T(")");
    if (restrictProc != _T("-"))
      sql << _T(",\n\tRESTRICT = ") << restrictProc;
    if (joinProc != _T("-"))
      sql << _T(",\n\tJOIN = ") << joinProc;
    if (canHash)
      sql << _T(",\n\tHASHES");
    if (canMerge)
      sql << _T(",\n\tMERGES");
    sql << _T(")");
    *output++ = sql;

    if (!description.empty()) {
      wxString commentSql;
      commentSql << _T("COMMENT ON OPERATOR ")
                 << qualifiedName << _T("(")
                 << (kind == _T("b") || kind == _T("l") ? leftTypeName : _T("none"))
                 << _T(", ")
                 << (kind == _T("b") || kind == _T("r") ? rightTypeName : _T("none"))
                 << _T(")")
                 << _T(" IS ") << QuoteLiteral(description);
      *output++ = commentSql;
    }
  }
    break;
  case Alter: {
    wxString kind = detail.ReadText(3);
    wxString leftTypeName = detail.ReadText(6);
    wxString rightTypeName = detail.ReadText(7);
    wxString description = detail.ReadText(14);
    wxString identification;
    identification << qualifiedName << _T("(")
                   << (kind == _T("b") || kind == _T("l") ? leftTypeName : _T("none"))
                   << _T(", ")
                   << (kind == _T("b") || kind == _T("r") ? rightTypeName : _T("none"))
                   << _T(")");
    *output++ = _T("ALTER OPERATOR ") + identification + _T(" OWNER TO ") + owner;
    *output++ = _T("ALTER OPERATOR ") + identification + _T(" SET SCHEMA ") + schemaName;
    if (!description.empty()) {
      *output++ = _T("COMMENT ON OPERATOR ") + identification + _T(" IS ") + QuoteLiteral(description);
    }
  }
    break;
  case Drop: {
    wxString kind = detail.ReadText(3);
    wxString leftTypeName = detail.ReadText(6);
    wxString rightTypeName = detail.ReadText(7);
    wxString sql;
    sql << _T("DROP OPERATOR /* IF EXISTS */ ")
        << qualifiedName << _T("(")
        << (kind == _T("b") || kind == _T("l") ? leftTypeName : _T("none"))
        << _T(", ")
        << (kind == _T("b") || kind == _T("r") ? rightTypeName : _T("none"))
        << _T(")")
        << _T(" /* CASCADE */");
    *output++ = sql;
  }
    break;

  default:
    wxASSERT(false);
  }
}

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
