/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __static_resources_h
#define __static_resources_h

#include "wx/icon.h"
#include "wx/image.h"

class StaticResources {
public:
  static void Init()
  {
    RegisterMemoryResources();
    wxImage::AddHandler(new wxPNGHandler());
  }

  static wxImage LoadVFSImage(const wxString &vfilename)
  {
    wxFileSystem fs;
    wxFSFile *fsfile = fs.OpenFile(vfilename);
    wxASSERT_MSG(fsfile != NULL, vfilename);
    wxImage im;
    wxInputStream *stream = fsfile->GetStream();
    im.LoadFile(*stream, fsfile->GetMimeType());
    delete fsfile;
    return im;
  }

#ifndef __WXMSW__
  static wxIcon LoadVFSIcon(const wxString &vfilename)
  {
    wxIcon icon;
    icon.CopyFromBitmap(LoadVFSImage(vfilename));
    return icon;
  }
#endif

private:
  static void RegisterMemoryResources();
};

#endif

// Local Variables:
// mode: c++
// End:
