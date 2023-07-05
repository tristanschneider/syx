#pragma once

#include "StableElementID.h"
#include "Table.h"

struct StableElementID;
struct GameDB;

namespace Events {
  struct EventsImpl;
  struct EventsInstance {
    EventsInstance();
    ~EventsInstance();

    DBEvents publishedEvents;
    std::unique_ptr<EventsImpl> impl;
  };
  struct EventsRow : SharedRow<EventsInstance>{};

  void onNewElement(StableElementID e, GameDB game);
  void onMovedElement(StableElementID e, GameDB game);
  void onRemovedElement(StableElementID e, GameDB game);
  //Populate publishedEvents and clear the internally stored events
  void publishEvents(GameDB game);
  //This should only be done during event processing at the end of the frame
  const DBEvents& getPublishedEvents(GameDB game);
};