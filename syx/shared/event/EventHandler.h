#pragma once

class Event;
class EventBuffer;

class EventHandler {
public:
  using Callback = std::function<void(const Event&)>;

  void registerEventHandler(size_t type, Callback h);
  void registerGlobalHandler(Callback h);
  void handleEvents(const EventBuffer& buffer);

private:
  TypeMap<Callback> mEventHandlers;
  Callback mGlobalHandler;
};