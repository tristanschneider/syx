#pragma once

#include "StableElementID.h"
#include "Table.h"

struct AppTaskArgs;
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
    void* db{};
  };
  struct CreatePublisher : Publisher {
    void operator()(StableElementID id);
  };
  struct DestroyPublisher : Publisher {
    void operator()(StableElementID id);
  };
  struct MovePublisher : Publisher {
    void operator()(StableElementID source, UnpackedDatabaseElementID destination);
  };

  CreatePublisher createCreatePublisher(GameDB db);
  DestroyPublisher createDestroyPublisher(GameDB db);
  MovePublisher createMovePublisher(GameDB db);

  //Re-resolve the given command ids. This shouldn't be necessary for pre-table service listeners,
  //and is for the table service and any post service listeners since the elements may have moved
  void resolve(DBEvents::MoveCommand& cmd, const StableElementMappings& mappings);

  //TODO: this is simpler to process if they are all "moves" but new is a move from nothing to somthing and remove is the opposite
  void onNewElement(StableElementID e, AppTaskArgs& args);
  void onNewElement(StableElementID e, GameDB game);
  void onMovedElement(StableElementID src, StableElementID dst, AppTaskArgs& args);
  void onMovedElement(StableElementID src, UnpackedDatabaseElementID dst, AppTaskArgs& args);
  void onMovedElement(StableElementID src, StableElementID dst, GameDB game);
  void onMovedElement(StableElementID src, UnpackedDatabaseElementID dst, GameDB game);
  void onRemovedElement(StableElementID e, GameDB game);
  //Populate publishedEvents and clear the internally stored events
  void publishEvents(GameDB game);
  //This should only be done during event processing at the end of the frame
  const DBEvents& getPublishedEvents(GameDB game);
};