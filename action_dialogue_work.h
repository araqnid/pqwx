/**
 * @file
 * @author Steve Haslam <araqnid@googlemail.com>
 */

#ifndef __action_dialogue_work_h
#define __action_dialogue_work_h

#include "object_browser_database_work.h"

class ActionDialogueWork : public ObjectBrowserManagedWork {
public:
  ActionDialogueWork(TxMode txMode, const SqlDictionary& sqlDictionary) : ObjectBrowserManagedWork(txMode), sqlDictionary(sqlDictionary) {}
  const SqlDictionary &sqlDictionary;
private:
};

#endif

// Local Variables:
// mode: c++
// indent-tabs-mode: nil
// End:
