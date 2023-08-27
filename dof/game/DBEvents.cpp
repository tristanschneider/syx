#include "Precompile.h"
#include "DBEvents.h"

#include "Simulation.h"
#include "TableAdapters.h"

namespace Events {
  using LockT = std::lock_guard<std::mutex>;

  void Publisher::operator()(StableElementID id) {
    GameDatabase* game = static_cast<GameDatabase*>(db);
    publish(id, { *game });
  }

  Publisher createPublisher(void(*publish)(StableElementID, GameDB), GameDB db) {
    return { publish, &db.db };
  }

  MovePublisher createMovePublisher(GameDB db) {
    return { &onMovedElement, &db.db };
  }

  void MovePublisher::operator()(StableElementID src, UnpackedDatabaseElementID dst) {
    GameDatabase* game = static_cast<GameDatabase*>(db);
    publish(src, dst, { *game });
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

  EventsImpl& _get(GameDB game) {
    return *std::get<Events::EventsRow>(std::get<GlobalGameData>(game.db.mTables).mRows).at().impl;
  }

  EventsInstance& _getInstance(GameDB game) {
    return std::get<Events::EventsRow>(std::get<GlobalGameData>(game.db.mTables).mRows).at();
  }

  EventsContext _getContext(GameDB game) {
    auto& impl = _get(game);
    return { impl, LockT{ impl.mutex } };
  }

  void onNewElement(StableElementID e, GameDB game) {
    EventsContext ctx{ _getContext(game) };
    ctx.impl.events.newElements.push_back(e);
  }

  void onMovedElement(StableElementID src, UnpackedDatabaseElementID dst, GameDB game) {
    EventsContext ctx{ _getContext(game) };
    ctx.impl.events.toBeMovedElements.push_back({ src, dst });
  }

  void onRemovedElement(StableElementID e, GameDB game) {
    EventsContext ctx{ _getContext(game) };
    ctx.impl.events.toBeRemovedElements.push_back(e);
  }

  DBEvents readEvents(GameDB game) {
    EventsContext ctx{ _getContext(game) };
    return ctx.impl.events;
  }

  void clearEvents(GameDB game) {
    EventsContext ctx{ _getContext(game) };
    ctx.impl.events.toBeMovedElements.clear();
    ctx.impl.events.newElements.clear();
    ctx.impl.events.toBeRemovedElements.clear();
  }

  //TODO: should these write invalid id on failure instead of leaving it unchaged?
  void resolve(std::vector<StableElementID>& id, const StableElementMappings& mappings) {
    for(StableElementID& i : id) {
      i = StableOperations::tryResolveStableID(i, mappings).value_or(i);
    }
  }

  void resolve(std::vector<DBEvents::MoveCommand>& cmd, const StableElementMappings& mappings) {
    for(auto& c : cmd) {
      c.source = StableOperations::tryResolveStableID(c.source, mappings).value_or(c.source);
    }
  }

  void publishEvents(GameDB game) {
    EventsInstance& instance = _getInstance(game);
    {
      EventsContext ctx{ _getContext(game) };
      instance.publishedEvents.toBeMovedElements.swap(ctx.impl.events.toBeMovedElements);
      instance.publishedEvents.newElements.swap(ctx.impl.events.newElements);
      instance.publishedEvents.toBeRemovedElements.swap(ctx.impl.events.toBeRemovedElements);

      ctx.impl.events.toBeMovedElements.clear();
      ctx.impl.events.newElements.clear();
      ctx.impl.events.toBeRemovedElements.clear();
    }
    StableElementMappings& mappings = TableAdapters::getStableMappings(game);
    resolve(instance.publishedEvents.toBeMovedElements, mappings);
    resolve(instance.publishedEvents.newElements, mappings);
    resolve(instance.publishedEvents.toBeRemovedElements, mappings);
  }

  const DBEvents& getPublishedEvents(GameDB game) {
    //Don't need to lock because this is supposed to be during the synchronous portion of the frame
    return _getInstance(game).publishedEvents;
  }
};