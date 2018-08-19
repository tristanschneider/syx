#include "event/Event.h"

class PickObjectEvent : public Event {
public:
  PickObjectEvent(Handle obj);
  Handle mObj;
};
