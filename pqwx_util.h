/**
 * @file
 * Various utility classes
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __pqwx_util_h
#define __pqwx_util_h

/**
 * An output iterator that appends values to a wxString.
 *
 * Very like ostream_iterator, including taking a delimiter parameter added after each item.
 */
class WxStringConcatenator : public std::iterator<std::output_iterator_tag,void,void,void,void> {
public:
  WxStringConcatenator(wxString &target, const wxString& delimiter = wxEmptyString) : target(target), delimiter(delimiter) {}

  WxStringConcatenator& operator=(const wxString& value)
  {
    target << value << delimiter;
    return *this;
  }
  WxStringConcatenator& operator*() { return *this; }
  WxStringConcatenator& operator++() { return *this; }
  WxStringConcatenator operator++(int) { return *this; }

private:
  wxString &target;
  wxString delimiter;
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
