// -*- c++ -*-

#ifndef __lazy_loader_h
#define __lazy_loader_h

class LazyLoader : public wxTreeItemData {
public:
  virtual void load(wxTreeItemId parent) = 0;
};

#endif
