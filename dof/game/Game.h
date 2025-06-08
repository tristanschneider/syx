#pragma once

#include "IAppModule.h"

class IGame;
struct ThreadLocalsInstance;
struct Scheduler;
struct IDatabase;
struct ThreadLocals;

namespace Input {
  class InputMapper;
}

class IRenderingModule : public IAppModule {
public:
  virtual void renderOnlyUpdate(IAppBuilder&) {}
  virtual void preSimUpdate(IAppBuilder&) {}
  virtual void postSimUpdate(IAppBuilder&) {}
};

struct MultithreadedDeps {
  explicit operator bool() const {
    return tls && scheduler;
  }

  ThreadLocals* tls{};
  Scheduler* scheduler{};
};

class IGameDatabaseReader {
public:
  virtual ~IGameDatabaseReader() = default;
  virtual MultithreadedDeps getMultithreadedDeps(IDatabase&) = 0;
};

//The implementation of the game itself is intended to have minimal knowledge of the simulation details, that's the responsibility of the modules
//Similarly, it shouldn't have knowledge of the contents of its database, although the "defaults" are exposed here for convenience
namespace Game {
  struct GameArgs {
    GameArgs();
    GameArgs(GameArgs&&);
    ~GameArgs();

    std::unique_ptr<IRenderingModule> rendering;
    std::vector<std::unique_ptr<IAppModule>> modules;
    std::unique_ptr<IGameDatabaseReader> dbSource;
  };

  std::unique_ptr<IGame> createGame(GameArgs&& args);
}

//TODO: Ideally not necessary to expose simulation here, requires more modularization of simulation
namespace Simulation {
  struct UpdateConfig;
}

namespace GameDefaults {
  class DefaultGameDatabaseReader : public IGameDatabaseReader {
  public:
    virtual MultithreadedDeps getMultithreadedDeps(IDatabase&);
  };

  struct DefaultGameBuilder {
    Game::GameArgs build()&&;

    std::unique_ptr<IGameDatabaseReader> dbSource;
    std::unique_ptr<IAppModule> initialEventValidator,
      physics,
      simulation,
      fragmentSpawner,
      sceneNavigator,
      sceneList,
      respawnArea,
      basicLoaders,
      reflection,
      physicsTest,
      finalEventValidator,
      events;
  };

  DefaultGameBuilder createDefaultGameBuilder();
  Game::GameArgs createDefaultGameArgs();
  Game::GameArgs createDefaultGameArgs(const Simulation::UpdateConfig& config);
}