#pragma once

#include "StableElementID.h"
#include "Table.h"

struct AppTaskArgs;
class IAppBuilder;
class RuntimeDatabaseTaskBuilder;

namespace Events {
  struct EventsInstance {
    EventsInstance();
    ~EventsInstance();

    DBEvents publishedEvents;
    std::unique_ptr<IDBEvents> impl;
  };
  struct EventsRow : SharedRow<EventsInstance>{};


  struct Publisher {
    AppTaskArgs* args{};
  };
  struct CreatePublisher : Publisher {
    void operator()(const ElementRef& id);
  };
  struct DestroyPublisher : Publisher {
    void operator()(const ElementRef& id);
  };
  struct MovePublisher : Publisher {
    void operator()(const ElementRef& source, const TableID& destination);
  };

  //Re-resolve the given command ids. This shouldn't be necessary for pre-table service listeners,
  //and is for the table service and any post service listeners since the elements may have moved
  void resolve(DBEvents::MoveCommand& cmd, const StableElementMappings& mappings);

  void onNewElement(const ElementRef& e, AppTaskArgs& args);
  void onMovedElement(const ElementRef& src, const TableID& dst, AppTaskArgs& args);
  void onRemovedElement(const ElementRef& e, AppTaskArgs& args);
  //Populate publishedEvents and clear the internally stored events
  void publishEvents(IAppBuilder& builder);

  //This should only be done during event processing at the end of the frame
  const DBEvents& getPublishedEvents(RuntimeDatabaseTaskBuilder& task);
};