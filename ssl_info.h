/**
 * @file
 * SSL Information
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __ssl_info_h
#define __ssl_info_h

#include <openssl/ssl.h>

/**
 * SSL information about a connection.
 */
class SSLInfo {
public:
  enum VerificationStatus { VERIFIED, SELF_SIGNED, FAILED };

  SSLInfo(SSL *ssl);

  const wxString& PeerDN() const { return peerDN; }
  const std::vector< std::pair<wxString,wxString> >& PeerDNStructure() const { return peerDNstructure; }
  const wxString& Protocol() const { return protocol; }
  const wxString& Cipher() const { return cipher; }
  int CipherBits() const { return bits; }
  VerificationStatus GetVerificationStatus() const { return verificationStatus; }

private:
  wxString peerDN;
  wxString protocol;
  wxString cipher;
  int bits;
  VerificationStatus verificationStatus;
  std::vector< std::pair<wxString,wxString> > peerDNstructure;
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
