#pragma once

class EventListener;

using MessageQueue = Guarded<EventListener, SpinLock>;

class MessageQueueProvider {
public:
  virtual MessageQueue getMessageQueue() = 0;
};