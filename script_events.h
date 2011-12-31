// -*- mode: c++ -*-

#ifndef __script_events_h
#define __script_events_h

BEGIN_DECLARE_EVENT_TYPES()
  DECLARE_EVENT_TYPE(PQWX_SCRIPT_TO_WINDOW, -1)
END_DECLARE_EVENT_TYPES()

#define EVT_SCRIPT_TO_WINDOW(id, fn) EVT_COMMAND(id, PQWX_SCRIPT_TO_WINDOW, fn)

#endif
