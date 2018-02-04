#pragma once

class EventBuffer;

using MessageQueue = Guarded<EventBuffer, SpinLock>;

class MessageQueueProvider {
public:
  virtual MessageQueue getMessageQueue() = 0;
};