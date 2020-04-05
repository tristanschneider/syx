#pragma once
#include "Util.h"

class EventBuffer;
class SpinLock;

using MessageQueue = Guarded<EventBuffer, SpinLock>;

class MessageQueueProvider {
public:
  virtual MessageQueue getMessageQueue() = 0;
};