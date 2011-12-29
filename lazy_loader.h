// -*- c++ -*-

#ifndef __lazy_loader_h
#define __lazy_loader_h

class LazyLoader : public wxTreeItemData {
public:
  // returns TRUE if lazy loading begun, or FALSE if loading completed
  virtual bool load(wxTreeItemId parent) = 0;
};

#endif
