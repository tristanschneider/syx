#include "Precompile.h"
#include "DBEvents.h"

#include "Simulation.h"
#include "TableAdapters.h"

namespace Events {
  using LockT = std::lock_guard<std::mutex>;

  void CreatePublisher::operator()(StableElementID id) {
    GameDatabase* game = static_cast<GameDatabase*>(db);
    onNewElement(id, { *game });
  }

  void DestroyPublisher::operator()(StableElementID id) {
    GameDatabase* game = static_cast<GameDatabase*>(db);
    onRemovedElement(id, { *game });
  }

  void MovePublisher::operator()(StableElementID source, UnpackedDatabaseElementID destination) {
    GameDatabase* game = static_cast<GameDatabase*>(db);
    onMovedElement(source, destination, { *game });
  }

  CreatePublisher createCreatePublisher(GameDB db) {
    return { &db.db };
  }

  DestroyPublisher createDestroyPublisher(GameDB db) {
    return { &db.db };
  }

  MovePublisher createMovePublisher(GameDB db) {
    return { &db.db };
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
    onMovedElement(StableElementID::invalid(), e, game);
  }

  void onMovedElement(StableElementID src, UnpackedDatabaseElementID dst, GameDB game) {
    //Create a "stable" id where the stable part is empty but the unstable part has the destination table id
    onMovedElement(src, StableElementID{ dst.mValue, dbDetails::INVALID_VALUE }, game);
  }

  void onMovedElement(StableElementID src, StableElementID dst, GameDB game) {
    EventsContext ctx{ _getContext(game) };
    ctx.impl.events.toBeMovedElements.push_back({ src, dst });
  }

  void onRemovedElement(StableElementID e, GameDB game) {
    onMovedElement(e, StableElementID::invalid(), game);
  }

  DBEvents readEvents(GameDB game) {
    EventsContext ctx{ _getContext(game) };
    return ctx.impl.events;
  }

  void clearEvents(GameDB game) {
    EventsContext ctx{ _getContext(game) };
    ctx.impl.events.toBeMovedElements.clear();
  }

  void resolve(StableElementID& id, const StableElementMappings& mappings) {
    //TODO: should these write invalid id on failure instead of leaving it unchaged?
    //Check explicitly for the stable part to ignore the move special case where a destination unstable type is used
    //This also ignores the create and destroy cases which contain empty ids
    if(id.mStableID != dbDetails::INVALID_VALUE) {
      id = StableOperations::tryResolveStableID(id, mappings).value_or(id);
    }
  }

  void resolve(DBEvents::MoveCommand& cmd, const StableElementMappings& mappings) {
      resolve(cmd.source, mappings);
      resolve(cmd.destination, mappings);
  }

  void resolve(std::vector<DBEvents::MoveCommand>& cmd, const StableElementMappings& mappings) {
    for(auto& c : cmd) {
      resolve(c, mappings);
    }
  }

  void publishEvents(GameDB game) {
    EventsInstance& instance = _getInstance(game);
    {
      EventsContext ctx{ _getContext(game) };
      instance.publishedEvents.toBeMovedElements.swap(ctx.impl.events.toBeMovedElements);

      ctx.impl.events.toBeMovedElements.clear();
    }
    StableElementMappings& mappings = TableAdapters::getStableMappings(game);
    resolve(instance.publishedEvents.toBeMovedElements, mappings);
  }

  const DBEvents& getPublishedEvents(GameDB game) {
    //Don't need to lock because this is supposed to be during the synchronous portion of the frame
    return _getInstance(game).publishedEvents;
  }
};