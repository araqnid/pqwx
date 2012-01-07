/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __lazy_loader_h
#define __lazy_loader_h

/**
 * Implemented by a tree item that is a marker for lazily-loaded data.
 */
class LazyLoader : public wxTreeItemData {
public:
  /**
   * Load the data covered by this marker.
   * @return TRUE if lazy loading begun, or FALSE if loading completed
   */
  virtual bool load(wxTreeItemId parent) = 0;
};

#endif

// Local Variables:
// mode: c++
// End:
