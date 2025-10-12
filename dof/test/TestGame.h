#pragma once

#include "SceneNavigator.h"
#include "Simulation.h"
#include "ThreadLocals.h"
#include <IAppModule.h>

struct IDatabase;
class IGame;

//Exposes creation of the game (simulation.cpp) with some accommodations for testing
//This is for integration scenarios of the way the game is registered.
//For testing the app framework itself, TestApp can be used
namespace Test {
  struct GameArgs {
    size_t fragmentCount{};
    size_t completedFragmentCount{};
    //Optionally create a player, and if so, at this position
    std::optional<glm::vec2> playerPos;
    //Boundary of broadphase and forces. Default should be enough to keep it out of the way
    glm::vec2 boundaryMin{ -100 };
    glm::vec2 boundaryMax{ 100 };
    bool enableFragmentGoals{ false };
    std::optional<size_t> forcedPadding;
  };

  //Hack for some funky ordering, these are for construct, GameArgs are for init
  struct GameConstructArgs {
    Config::GameConfig config;
    Config::PhysicsConfig physics;
    Simulation::UpdateConfig updateConfig;
    std::unique_ptr<SceneNavigator::IScene> scene;
    std::vector<std::unique_ptr<IAppModule>> modules;
  };

  struct KnownTables {
    KnownTables() = default;
    KnownTables(IAppBuilder& builder);
    TableID player;
    TableID fragments;
    TableID completedFragments;
  };

  struct TestGame {
    TestGame(GameConstructArgs args = {});
    TestGame(const GameArgs& args);
    TestGame(std::unique_ptr<SceneNavigator::IScene> scene);
    TestGame(std::unique_ptr<IGame> _game);
    ~TestGame();

    RuntimeDatabaseTaskBuilder& builder();
    AppTaskArgs& sharedArgs();
    void init(const GameArgs& args);
    void update();
    void execute(std::unique_ptr<IAppBuilder> toExecute);
    void execute(TaskRange range);
    ElementRef getFromTable(const TableID& id, size_t index);

    void loadSceneFromFile(std::string_view file);

    std::unique_ptr<IGame> game;
    std::unique_ptr<IAppBuilder> testBuilder;
    std::unique_ptr<RuntimeDatabaseTaskBuilder> test;
    std::unique_ptr<AppTaskArgs> taskArgs;
    KnownTables tables;
  };
}