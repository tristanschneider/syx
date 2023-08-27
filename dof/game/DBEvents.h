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

  struct Publisher {
    void operator()(StableElementID id);
    void (*publish)(StableElementID, GameDB);
    void* db{};
  };
  struct MovePublisher {
    void operator()(StableElementID src, UnpackedDatabaseElementID dst);
    void (*publish)(StableElementID, UnpackedDatabaseElementID, GameDB);
    void* db{};
  };

  Publisher createPublisher(void(*publish)(StableElementID, GameDB), GameDB db);
  MovePublisher createMovePublisher(GameDB db);
  //TODO: this is simpler to process if they are all "moves" but new is a move from nothing to somthing and remove is the opposite
  void onNewElement(StableElementID e, GameDB game);
  void onMovedElement(StableElementID src, UnpackedDatabaseElementID dst, GameDB game);
  void onRemovedElement(StableElementID e, GameDB game);
  //Populate publishedEvents and clear the internally stored events
  void publishEvents(GameDB game);
  //This should only be done during event processing at the end of the frame
  const DBEvents& getPublishedEvents(GameDB game);
};