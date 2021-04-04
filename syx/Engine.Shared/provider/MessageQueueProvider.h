#pragma once
#include "threading/SpinLock.h"
#include "Util.h"

class DeferredEventBuffer;
class EventBuffer;
class SpinLock;

using MessageQueue = Guarded<EventBuffer, SpinLock>;
using DeferredMessageQueue = Guarded<DeferredEventBuffer, std::mutex>;

class MessageQueueProvider {
public:
  virtual MessageQueue getMessageQueue() = 0;
  virtual DeferredMessageQueue getDeferredMessageQueue() = 0;
};