#include "Precompile.h"
#include "Simulation.h"

#include "config/ConfigIO.h"
#include "EnkiLocalScheduler.h"
#include "File.h"
#include "Fragment.h"
#include "GameplayExtract.h"
#include "ILocalScheduler.h"
#include "stat/AllStatEffects.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "Profile.h"
#include "PhysicsSimulation.h"
#include "Player.h"
#include "DebugInput.h"
#include "ability/PlayerAbility.h"
#include "AppBuilder.h"
#include "FragmentStateMachine.h"
#include "SpatialQueries.h"
#include "ConstraintSolver.h"
#include "GameDatabase.h"
#include "SceneNavigator.h"
#include "loader/AssetService.h"
#include "CommonTasks.h"
#include "IAppModule.h"

ThreadLocalsInstance::ThreadLocalsInstance() = default;
ThreadLocalsInstance::~ThreadLocalsInstance() = default;

void Simulation::buildUpdateTasks(IAppBuilder& builder, const UpdateConfig& config) {
  Loader::processRequests(builder);

  PhysicsSimulation::updatePhysics(builder);
  config.injectGameplayTasks(builder);
  DebugInput::updateDebugCamera(builder);
  Player::updateInput(builder);
  Fragment::updateFragmentGoals(builder);

  if(config.enableFragmentStateMachine) {
    FragmentStateMachine::update(builder);
  }
  //At the end of gameplay, turn any gameplay impulses into stat effects
  GameplayExtract::applyGameplayImpulses(builder);

  SpatialQuery::gameplayUpdateQueries(builder);
  StatEffect::createTasks(builder);

  CommonTasks::migrateThreadLocalDBsToMain(builder);
}

void tryInitFromConfig(Config::GameConfig& toSet, const ConfigIO::Result::Error& error) {
  printf("Error reading config file, initializing with defaults. [%s]\n", error.message.c_str());
  ConfigIO::defaultInitialize(*Config::createFactory(), toSet);
}

void tryInitFromConfig(Config::GameConfig& toSet, Config::GameConfig&& loaded) {
  printf("Config found, initializing from file.\n");
  toSet = std::move(loaded);
}

const char* Simulation::getConfigName() {
  return "config.json";
}

void Simulation::initScheduler(IAppBuilder& builder, const ThreadLocalDatabaseFactory& f) {
  auto task = builder.createTask();
  task.setName("init scheduler");
  Scheduler* scheduler = task.query<SharedRow<Scheduler>>().tryGetSingletonElement();
  ThreadLocalsInstance* tls = task.query<ThreadLocalsRow>().tryGetSingletonElement();
  StableElementMappings* mappings = &task.getDatabase().getMappings();
  task.setCallback([scheduler, tls, mappings, f](AppTaskArgs&) {
    enki::TaskSchedulerConfig cfg;
    struct ProfileStop {
      static void threadStop(uint32_t) {
        ON_PROFILE_THREAD_DESTROYED;
      }
    };
    cfg.profilerCallbacks.threadStop = &ProfileStop::threadStop;
    cfg.numTaskThreadsToCreate = std::min(static_cast<uint32_t>(16), cfg.numTaskThreadsToCreate);

    scheduler->mScheduler.Initialize(cfg);
    tls->instance = std::make_unique<ThreadLocals>(
      scheduler->mScheduler.GetNumTaskThreads(),
      mappings,
      //ThreadLocals only uses this during the constructor
      Tasks::createEnkiSchedulerFactory(*scheduler, [tls](size_t t) { return tls->instance->get(t); }).get(),
      f
    );
  });
  builder.submitTask(std::move(task));
}

void Simulation::init(IAppBuilder& builder) {
  GameDatabase::configureDefaults(builder);
  StatEffect::init(builder);
  Player::init(builder);
}

void resetInput(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("reset input");
  auto input = task.query<GameInput::PlayerKeyboardInputRow>();
  task.setCallback([input](AppTaskArgs&) mutable {
    input.forEachElement([](GameInput::PlayerKeyboardInput& input) {
      input.mRawKeys.clear();
      input.mRawWheelDelta = 0.0f;
      input.mRawMouseDeltaPixels = glm::vec2{ 0.0f };
      input.mRawText.clear();
    });
  });
  builder.submitTask(std::move(task));
}

class ConfigModule : public IAppModule {
public:
  //Read config file to populate Config::GameConfig which other modules can read in their init.
  //As such, this module is registered early.
  void init(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("load config");
    Config::GameConfig* gameConfig = TableAdapters::getGameConfigMutable(task);
    FileSystem* fs = task.query<SharedRow<FileSystem>>().tryGetSingletonElement();
    task.setCallback([gameConfig, fs](AppTaskArgs&) mutable {
      if(std::optional<std::string> buffer = fs ? File::readEntireFile(*fs, Simulation::getConfigName()) : std::nullopt) {
        ConfigIO::Result result = ConfigIO::deserializeJson(*buffer, *Config::createFactory());
        std::visit([&](auto&& r) { tryInitFromConfig(*gameConfig, std::move(r)); }, std::move(result.value));
      }
      else if(gameConfig) {
        tryInitFromConfig(*gameConfig, ConfigIO::Result::Error{});
      }
    });
    builder.submitTask(std::move(task));
  }
};

std::unique_ptr<IAppModule> Simulation::createConfigModule() {
  return std::make_unique<ConfigModule>();
}

class GameplayExtractModule : public IAppModule {
public:
  void update(IAppBuilder& builder) final {
    GameplayExtract::extractGameplayData(builder);
  }
};

std::unique_ptr<IAppModule> Simulation::createGameplayExtract() {
  return std::make_unique<GameplayExtractModule>();
}

class SimulationModule : public IAppModule {
public:
  SimulationModule(Simulation::UpdateConfig&& cfg)
    : config{ std::move(cfg) }
  {
  }

  void createDatabase(RuntimeDatabaseArgs& args) final {
    GameDatabase::create(args);
  }

  void initScheduler(IAppBuilder& builder, const ThreadLocalDatabaseFactory& f) final {
    Simulation::initScheduler(builder, f);
  }

  void init(IAppBuilder& builder) final {
    Simulation::init(builder);
    GameInput::init(builder);
  }

  void update(IAppBuilder& builder) final {
    Simulation::buildUpdateTasks(builder, config);
    resetInput(builder);
    GameInput::update(builder);
  }

  virtual void preProcessEvents(IAppBuilder& builder) {
    Fragment::preProcessEvents(builder);
    FragmentStateMachine::preProcessEvents(builder);
  }

  Simulation::UpdateConfig config;
};

std::unique_ptr<IAppModule> Simulation::createModule(UpdateConfig cfg) {
  return std::make_unique<SimulationModule>(std::move(cfg));
}
