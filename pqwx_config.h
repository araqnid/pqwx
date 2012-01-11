/**
 * @file
 * Compile-time configuration preprocessor symbols
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __pqwx_config_h
#define __pqwx_config_h

#ifdef __WXMSW__
// select/pipe trickery not ported to Windows yet...
#else
#define PQWX_NOTIFICATION_MONITOR 1
#endif

#endif

// Local Variables:
// mode: c++
// End:
