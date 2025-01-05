#include "Precompile.h"
#include "DBEvents.h"

#include "TableAdapters.h"
#include "AppBuilder.h"
#include "ThreadLocals.h"

namespace Events {
  void CreatePublisher::operator()(const ElementRef& id) {
    onNewElement(id, *args);
  }

  void DestroyPublisher::operator()(const ElementRef& id) {
    onRemovedElement(id, *args);
  }

  void MovePublisher::operator()(const ElementRef& source, const TableID& destination) {
    onMovedElement(source, destination, *args);
  }

  struct EventsImpl : IDBEvents {
    void emit(DBEvents::MoveCommand&& e) final {
      std::lock_guard<std::mutex> lock{ mutex };
      events.toBeMovedElements.push_back(std::move(e));
    }

    void publishTo(DBEvents& destination) final {
      std::lock_guard<std::mutex> lock{ mutex };
      destination.toBeMovedElements.swap(events.toBeMovedElements);
      events.toBeMovedElements.clear();
    }

    DBEvents events;
    std::mutex mutex;
  };

  EventsInstance::EventsInstance()
    : impl(std::make_unique<EventsImpl>()) {
  }

  EventsInstance::~EventsInstance() = default;

  void pushEvent(const DBEvents::Variant& src, const DBEvents::Variant& dst, AppTaskArgs& args) {
    args.getEvents()->emit({ src, dst });
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

  void publishEvents(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("publish events");
    EventsInstance& instance = *task.query<EventsRow>().tryGetSingletonElement();
    task.setCallback([&instance](AppTaskArgs& args) {
      args.getEvents()->publishTo(instance.publishedEvents);
    });
    builder.submitTask(std::move(task));
  }

  const DBEvents& getPublishedEvents(RuntimeDatabaseTaskBuilder& task) {
    return task.query<const EventsRow>().tryGetSingletonElement()->publishedEvents;
  }
};