#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppBuilder.h"
#include "Game.h"
#include "IGame.h"
#include "RuntimeDatabase.h"
#include "Database.h"
#include "SceneNavigator.h"
#include "GameDatabase.h"
#include "RowTags.h"
#include "TLSTaskImpl.h"
#include "SpatialPairsStorage.h"
#include "DBEvents.h"
#include "stat/ConstraintStatEffect.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(SceneTransition) {
    struct TestScenes {
      static TestScenes* get(RuntimeDatabaseTaskBuilder& task) {
        return task.query<SharedRow<TestScenes>>().tryGetSingletonElement();
      }

      SceneNavigator::SceneID empty{};
      SceneNavigator::SceneID physics{};
      SceneNavigator::SceneID multithreaded{};
      SceneNavigator::SceneID physicsModifier{};
      std::vector<ElementRef> objects;
      std::mutex objsMutex;
    };

    struct PhysicsScene : SceneNavigator::IScene {
      struct InitTaskLocal {
        InitTaskLocal(RuntimeDatabase& db)
          : objects{ db.tryGet(GameDatabase::Tables{ db }.physicsObjsWithZ) }
          , stable{ objects->tryGet<StableIDRow>() }
        {}
        InitTaskLocal(AppTaskArgs& args)
          : InitTaskLocal{ args.getLocalDB() }
        {}

        RuntimeTable* objects{};
        StableIDRow* stable{};
      };

      struct InitTask {
        InitTask(RuntimeDatabaseTaskBuilder& task)
          : scenes{ TestScenes::get(task) }
        {
        }

        void execute(InitTaskLocal& locals, AppTaskArgs& args) {
          const size_t count = 5;
          Assert::AreEqual(size_t(0), locals.objects->size());
          size_t b = locals.objects->addElements(count);
          args.getLocalDB().setTableDirty(locals.objects->getID());
          ConstraintStatEffect::Builder builder{ args };
          std::vector<ElementRef> objs;
          for(size_t i = 0; i < count; ++i) {
            objs.push_back(locals.stable->at(b + i));
            builder.createStatEffects(1).setLifetime(4);
            builder.constraintBuilder().setJointType({ Constraints::MotorJoint{
              .linearTarget = glm::vec2{ 0.1f },
              .linearForce = 1.f,
            }}).setTargets(objs.back(), {});
          }

          builder.createStatEffects(1).setLifetime(99);
          builder.constraintBuilder().setJointType({ Constraints::WeldJoint{
            .localCenterToPinA = glm::vec2{ 1, 0 },
            .localCenterToPinB = glm::vec2{ -1, 0 }
          }}).setTargets(objs[1], objs[3]);

          {
            std::lock_guard<std::mutex> g{ scenes->objsMutex };
            scenes->objects.insert(scenes->objects.end(), objs.begin(), objs.end());
          }
        }

        TestScenes* scenes{};
      };

      void init(IAppBuilder& builder) {
        auto task = TLSTask::create<InitTask, DefaultTaskGroup, InitTaskLocal>("init");
        task->setPinning(AppTaskPinning::ThreadID{ 2 });
        builder.submitTask(std::move(task));
      }
    };

    //Multithreaded use of thread local db to add elements
    struct MultithreadedScene : SceneNavigator::IScene {
      struct ConfigTaskGroup {
        ConfigTaskGroup(RuntimeDatabaseTaskBuilder&, std::shared_ptr<AppTaskConfig> cfg)
          : config{ cfg } {
        }

        std::shared_ptr<AppTaskConfig> config;
      };
      struct ConfigTask {
        ConfigTask(RuntimeDatabaseTaskBuilder& task) {
          //Force dependency for child tasks
          TestScenes::get(task);
        }

        void execute(ConfigTaskGroup& group, AppTaskArgs&) {
          group.config->setSize(AppTaskSize{ .workItemCount = 10, .batchSize = 1 });
        }
      };

      void init(IAppBuilder& builder) {
        auto task = TLSTask::create<PhysicsScene::InitTask, DefaultTaskGroup, PhysicsScene::InitTaskLocal>("mt");
        auto cfg = task->getOrAddConfig();
        auto configTask = TLSTask::createWithArgs<ConfigTask, ConfigTaskGroup, DefaultTaskLocals>("cfg", cfg);

        builder.submitTask(std::move(configTask));
        builder.submitTask(std::move(task));
      }
    };

    //Pinned use of modifier to add elements
    struct PhysicsModifierScene : SceneNavigator::IScene {
      struct InitTask {
        InitTask(RuntimeDatabaseTaskBuilder& task)
          : scenes{ TestScenes::get(task) }
          , table{ GameDatabase::Tables{ task }.physicsObjsWithZ }
          , modifier{ task.getModifierForTable(table) }
          , stable{ task.query<const StableIDRow>(table).tryGet<0>(0) }
        {
        }

        void execute(AppTaskArgs& args) {
          const size_t count = 5;
          size_t b = modifier->addElements(count);
          ConstraintStatEffect::Builder builder{ args };
          std::vector<ElementRef> objs;
          for(size_t i = 0; i < count; ++i) {
            objs.push_back(stable->at(b + i));
            Events::onNewElement(objs.back(), args);

            //builder.createStatEffects(1).setLifetime(4);
            //builder.constraintBuilder().setJointType({ Constraints::MotorJoint{
            //  .linearTarget = glm::vec2{ 0.1f },
            //  .linearForce = 1.f,
            //}}).setTargets(objs.back(), {});
          }

          //builder.createStatEffects(1).setLifetime(99);
          //builder.constraintBuilder().setJointType({ Constraints::WeldJoint{
          //  .localCenterToPinA = glm::vec2{ 1, 0 },
          //  .localCenterToPinB = glm::vec2{ -1, 0 }
          //}}).setTargets(objs[1], objs[3]);

          {
            std::lock_guard<std::mutex> g{ scenes->objsMutex };
            scenes->objects.insert(scenes->objects.end(), objs.begin(), objs.end());
          }
        }

        TableID table;
        TestScenes* scenes{};
        std::shared_ptr<ITableModifier> modifier;
        const StableIDRow* stable{};
      };

      void init(IAppBuilder& builder) {
        auto task = TLSTask::create<InitTask>("init");
        task->setPinning(AppTaskPinning::ThreadID{ 2 });
        builder.submitTask(std::move(task));
      }
    };

    struct TestModule : IAppModule {
      struct RegisterScenes {
        RegisterScenes(RuntimeDatabaseTaskBuilder& task)
          : registry{ SceneNavigator::createRegistry(task) }
          , scenes{ TestScenes::get(task) }
        {
        }

        void execute(AppTaskArgs&) {
          scenes->empty = registry->registerScene(std::make_unique<SceneNavigator::IScene>());
          scenes->physics = registry->registerScene(std::make_unique<PhysicsScene>());
          scenes->multithreaded = registry->registerScene(std::make_unique<MultithreadedScene>());
          scenes->physicsModifier = registry->registerScene(std::make_unique<PhysicsModifierScene>());
        }

        std::shared_ptr<SceneNavigator::ISceneRegistry> registry;
        TestScenes* scenes{};
      };

      void init(IAppBuilder& builder) final {
        builder.submitTask(TLSTask::create<RegisterScenes>("reg"));
      }

      void createDatabase(RuntimeDatabaseArgs& args) {
        DBReflect::addDatabase<Database<Table<SharedRow<TestScenes>>>>(args);
      }
    };

    struct EventValidator : IAppModule {
      struct Base {
        Base(RuntimeDatabaseTaskBuilder& task)
          : events{ Events::getPublishedEvents(task) }
          , id{ task.getIDResolver()->getRefResolver() }
          , resolver{ task.getResolver(stable) }
        {}

        void assertExists(const ElementRef& e) {
          Assert::IsNotNull(e.tryGet());
          Assert::IsNotNull(resolver->tryGetOrSwapRowElement(stable, id.tryUnpack(e)));
        }

        const DBEvents& events;
        CachedRow<const StableIDRow> stable;
        ElementRefResolver id;
        std::shared_ptr<ITableResolver> resolver;
      };

      struct PreValidator : Base {
        using Base::Base;

        void execute(AppTaskArgs&) {
          for(const auto& cmd : events.toBeMovedElements) {
            auto from = std::get_if<ElementRef>(&cmd.source);
            auto to = std::get_if<ElementRef>(&cmd.destination);
            Assert::IsTrue(from || to, L"Event should always refer to an element");
            if(from) {
              //Move or destroy. The element should still exist because it hasn't happened yet
              assertExists(*from);
              Assert::IsNull(to, L"Event should only refer to a single element");
            }
            if(to) {
              //New element created, should exist as they are emitted upon creation of the element
              assertExists(*to);
              Assert::IsNull(from, L"Event should only refer to a single element");
            }
          }
        }
      };

      struct PostValidator : Base {
        using Base::Base;

        void execute(AppTaskArgs&) {
          for(const auto& cmd : events.toBeMovedElements) {
            auto from = std::get_if<ElementRef>(&cmd.source);
            auto to = std::get_if<ElementRef>(&cmd.destination);
            Assert::IsTrue(from || to);
            if(to) {
              //New element should exist
              assertExists(*to);
              Assert::IsNull(from);
            }
            if(from) {
              //If it moved, make sure it's at the specified table
              if(auto toID = std::get_if<TableID>(&cmd.destination)) {
                Assert::IsTrue(static_cast<bool>(*from));
                Assert::AreEqual(toID->getTableIndex(), from->getMapping()->getTableIndex());
              }
              //If it was a destroy command, ensure it was destroyed
              else if(std::holds_alternative<std::monostate>(cmd.destination)) {
                Assert::IsFalse(static_cast<bool>(*from));
              }
              Assert::IsNull(to);
            }
          }
        }
      };

      void preProcessEvents(IAppBuilder& builder) {
        builder.submitTask(TLSTask::create<PreValidator>("pre"));
      }

      void postProcessEvents(IAppBuilder& builder) {
        builder.submitTask(TLSTask::create<PostValidator>("post"));
      }
    };

    struct GameTask {
      GameTask(IGame& game)
        : db{ game.getDatabase().getRuntime() }
        , task{ db }
      {
        task.discard();
      }

      operator RuntimeDatabaseTaskBuilder&() {
        return task;
      }

      RuntimeDatabase& db;
      RuntimeDatabaseTaskBuilder task;
    };

    void navigateToScene(IGame& game, SceneNavigator::SceneID scene) {
      GameTask t{ game };
      auto navigator = SceneNavigator::createNavigator(t);
      navigator->navigateTo(scene);

      for(size_t i = 0; i < 4; ++i) {
        game.updateSimulation();
      }
    }

    void assertSceneObjectsExist(IGame& game, bool shouldExist) {
      GameTask t{ game };
      TestScenes* scenes = t.db.query<SharedRow<TestScenes>>().tryGetSingletonElement();

      CachedRow<Tags::PosXRow> row;
      auto res = t.task.getResolver(row);
      auto id = t.task.getIDResolver()->getRefResolver();
      for(const ElementRef& e : scenes->objects) {
        Assert::AreEqual(shouldExist, e.tryGet() != nullptr);
        Assert::AreEqual(shouldExist, res->tryGetOrSwapRowElement(row, id.tryUnpack(e)) != nullptr);
      }
    }

    struct PhysicsCounts {
      size_t objectCount{};
      std::optional<size_t> constraintCount{};
    };

    void assertPhysicsCounts(IGame& game, const PhysicsCounts& counts) {
      GameTask t{ game };
      auto modifier = SP::createStorageModifier(t.task);
      Assert::AreEqual(counts.objectCount, modifier->nodeCount());
      if(counts.constraintCount) {
        Assert::AreEqual(*counts.constraintCount, modifier->edgeCount());
      }
    }

    void assertNoPhysicsObjects(IGame& game) {
      assertPhysicsCounts(game, PhysicsCounts{
        .objectCount = 0,
        .constraintCount = 0
      });
    }

    TEST_METHOD(Basic) {
      Game::GameArgs args = GameDefaults::createDefaultGameArgs();
      args.modules.push_back(std::make_unique<TestModule>());
      args.modules.push_back(std::make_unique<EventValidator>());
      std::unique_ptr<IGame> game = Game::createGame(std::move(args));
      game->init();
      RuntimeDatabase& db = game->getDatabase().getRuntime();
      TestScenes* scenes = db.query<SharedRow<TestScenes>>().tryGetSingletonElement();

      assertNoPhysicsObjects(*game);

      //Back and forth between empty and physics
      for(int i = 0; i < 10; ++i) {
        navigateToScene(*game, scenes->physics);
        Assert::AreEqual(size_t(5), scenes->objects.size());
        assertSceneObjectsExist(*game, true);
        assertPhysicsCounts(*game, PhysicsCounts{
          //5 objects, 6 nodes created by constraints
          .objectCount = 11,
        });

        navigateToScene(*game, scenes->empty);
        assertSceneObjectsExist(*game, false);
        assertNoPhysicsObjects(*game);
        scenes->objects.clear();
      }

      //Reloading the physics scene on top of itself
      for(int i = 0; i < 10; ++i) {
        navigateToScene(*game, scenes->physics);
        Assert::AreEqual(size_t(5), scenes->objects.size());
        assertSceneObjectsExist(*game, true);
        assertPhysicsCounts(*game, PhysicsCounts{
          //5 objects, 6 nodes created by constraints
          .objectCount = 11,
        });
        scenes->objects.clear();
      }

      navigateToScene(*game, scenes->multithreaded);

      navigateToScene(*game, scenes->empty);

      navigateToScene(*game, scenes->physicsModifier);
    }

    TEST_METHOD(IDReuse) {
      std::unique_ptr<IGame> game = Game::createGame(GameDefaults::createDefaultGameArgs());
      game->init();
      RuntimeDatabase& db = game->getDatabase().getRuntime();
      GameDatabase::Tables t{ db };
      RuntimeTable* table = db.tryGet(t.physicsObjsWithZ);
      const StableIDRow* stable = table->tryGet<StableIDRow>();
      RuntimeDatabaseTaskBuilder task{ db };
      task.discard();
      ElementRefResolver id = task.getIDResolver()->getRefResolver();

      table->resize(1);
      Assert::IsTrue(table->getID() == TableID{ id.uncheckedUnpack(stable->at(0)) });

      table->resize(0);
      table->resize(1);
      Assert::IsTrue(table->getID() == TableID{ id.uncheckedUnpack(stable->at(0)) }, L"Table ID should ignore the version field");
    }
  };
}