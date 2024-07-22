#include "Precompile.h"
#include "DBEvents.h"

#include "TableAdapters.h"
#include "AppBuilder.h"
#include "ThreadLocals.h"

namespace Events {
  using LockT = std::lock_guard<std::mutex>;

  void CreatePublisher::operator()(const ElementRef& id) {
    onNewElement(id, *args);
  }

  void DestroyPublisher::operator()(const ElementRef& id) {
    onRemovedElement(id, *args);
  }

  void MovePublisher::operator()(const ElementRef& source, const TableID& destination) {
    onMovedElement(source, destination, *args);
  }

  struct EventsImpl {
    DBEvents events;
    std::mutex mutex;
  };

  EventsInstance::EventsInstance()
    : impl(std::make_unique<EventsImpl>()) {
  }

  EventsInstance::~EventsInstance() = default;

  struct EventsContext {
    EventsImpl& impl;
    LockT lock;
  };

  EventsContext _getContext(AppTaskArgs& args) {
    EventsImpl* impl = ThreadLocalData::get(args).events;
    return { *impl, LockT{ impl->mutex } };
  }

  void pushEvent(const DBEvents::Variant& src, const DBEvents::Variant& dst, AppTaskArgs& args) {
    EventsContext ctx{ _getContext(args) };
    ctx.impl.events.toBeMovedElements.push_back({ src, dst });
  }

  void onNewElement(const ElementRef& e, AppTaskArgs& args) {
    pushEvent({ std::monostate{} }, { e }, args);
  }

  void onMovedElement(const ElementRef& src, const TableID& dst, AppTaskArgs& args) {
    pushEvent({ src }, { dst }, args);
  }

  void onRemovedElement(const ElementRef& e, AppTaskArgs& args) {
    pushEvent({ e }, { std::monostate{} }, args);
  }

  //void resolve(StableElementID& id, IIDResolver& ids) {
  //  //TODO: should these write invalid id on failure instead of leaving it unchaged?
  //  //Check explicitly for the stable part to ignore the move special case where a destination unstable type is used
  //  //This also ignores the create and destroy cases which contain empty ids
  //  if(id.mStableID != dbDetails::INVALID_VALUE) {
  //    id = ids.tryResolveStableID(id).value_or(id);
  //  }
  //}

  //TODO: delete I think
  //void resolve(DBEvents::MoveCommand& cmd, IIDResolver& ids) {
  //    resolve(cmd.source, ids);
  //    resolve(cmd.destination, ids);
  //}
  //
  //void resolve(std::vector<DBEvents::MoveCommand>& cmd, IIDResolver& ids) {
  //  for(auto& c : cmd) {
  //    resolve(c, ids);
  //  }
  //}

  void publishEvents(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("publish events");
    EventsInstance& instance = *task.query<EventsRow>().tryGetSingletonElement();
    std::shared_ptr<IIDResolver> ids = task.getIDResolver();

    task.setCallback([&instance, ids](AppTaskArgs& args) {
      {
        EventsContext ctx{ _getContext(args) };
        instance.publishedEvents.toBeMovedElements.swap(ctx.impl.events.toBeMovedElements);

        ctx.impl.events.toBeMovedElements.clear();
      }
      //resolve(instance.publishedEvents.toBeMovedElements, *ids);
    });
    builder.submitTask(std::move(task));
  }

  const DBEvents& getPublishedEvents(RuntimeDatabaseTaskBuilder& task) {
    return task.query<const EventsRow>().tryGetSingletonElement()->publishedEvents;
  }
};