#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppContext.h"
#include "CommandBuffer.h"
#include "LinearView.h"
#include "Scheduler.h"
#include "System.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(AppContextTest) {
    inline static std::chrono::time_point<std::chrono::high_resolution_clock> currentTime;
    inline static size_t timeIncrement = 0;
    static std::chrono::time_point<std::chrono::high_resolution_clock> mockGetTime() {
      auto result = currentTime;
      currentTime += std::chrono::milliseconds(timeIncrement);
      return result;
    }

    static void setTimeMS(size_t ms) {
      currentTime = std::chrono::time_point<std::chrono::high_resolution_clock>(std::chrono::milliseconds(ms));
    }

    using TestEntity = LinearEntity;
    using TestEntityRegistry = EntityRegistry<TestEntity>;
    using TestEntityFactory = EntityFactory<TestEntity>;
    using TestScheduler = Scheduler<TestEntity, LockQueue>;
    struct TestTimer : public Timer<100, &mockGetTime> {
      using Timer::Timer;
      size_t _redeemTicks() {
        return redeemTicks(mockGetTime());
      }
    };
    template<class... Rest>
    using TestView = View<TestEntity, Rest...>;
    using TestSystem = std::shared_ptr<ISystem<TestEntity>>;

    enum class TestStage {
      A,
      B,
      C,
    };

    using TestAppContext = AppContext<TestScheduler, TestTimer, TestStage, TestEntity>;

    TEST_METHOD(Timer_DefaultConstruct_NoTicks) {
      TestTimer timer(1000);

      Assert::AreEqual(size_t(0), timer._redeemTicks());
    }

    TEST_METHOD(Timer_LessTimeElapsed_NoTicks) {
      setTimeMS(0);
      TestTimer timer(100);
      setTimeMS(5);

      Assert::AreEqual(size_t(0), timer._redeemTicks());
    }

    TEST_METHOD(Timer_TickTimeElapsed_OneTick) {
      setTimeMS(0);
      TestTimer timer(100);
      setTimeMS(10);

      Assert::AreEqual(size_t(1), timer._redeemTicks());
      Assert::AreEqual(size_t(0), timer._redeemTicks());
    }

    TEST_METHOD(Timer_TwoTicksElapsed_TwoTicks) {
      setTimeMS(0);
      TestTimer timer(100);
      setTimeMS(20);

      Assert::AreEqual(size_t(2), timer._redeemTicks());
      Assert::AreEqual(size_t(0), timer._redeemTicks());
    }

    TEST_METHOD(Timer_TickWithRemainder_RedeemedOnSecondTick) {
      setTimeMS(0);
      TestTimer timer(100);
      setTimeMS(15);
      timer._redeemTicks();
      setTimeMS(20);

      Assert::AreEqual(size_t(1), timer._redeemTicks());
      Assert::AreEqual(size_t(0), timer._redeemTicks());
    }

    TEST_METHOD(Timer_ZeroTarget_AlwaysOneTick) {
      TestTimer timer(0);

      Assert::AreEqual(size_t(1), timer._redeemTicks());
      Assert::AreEqual(size_t(1), timer._redeemTicks());
    }

    TEST_METHOD(Timer_OverCap_RedeemedOnNextTick) {
      setTimeMS(0);
      TestTimer timer(1000);
      setTimeMS(101);

      Assert::AreEqual(size_t(100), timer._redeemTicks());
      Assert::AreEqual(size_t(1), timer._redeemTicks());
      Assert::AreEqual(size_t(0), timer._redeemTicks());
    }

    struct PhaseTracker {
      std::vector<TestStage> mStagesTicked;
    };

    template<TestStage stage, size_t delayMS = 0>
    struct PhaseTrackerSystem {
      static void tick(SystemContext<TestEntity, TestView<Write<PhaseTracker>>>& context) {
        if constexpr(delayMS > 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(delayMS));
        }
        for(auto&& entity : context.get<TestView<Write<PhaseTracker>>>()) {
          entity.get<PhaseTracker>().mStagesTicked.push_back(stage);
        }
      }
    };

    struct RegistryWithPhaseTracker {
      RegistryWithPhaseTracker() {
        setTimeMS(0);
        mEntity = mRegistry.createEntity(*mRegistry.getDefaultEntityGenerator());
        mRegistry.addComponent<PhaseTracker>(mEntity);
      }

      void assertPhasesTicked(std::initializer_list<TestStage> phases) {
        if(PhaseTracker* tracker = mRegistry.tryGetComponent<PhaseTracker>(mEntity)) {
          Assert::IsTrue(tracker->mStagesTicked == std::vector<TestStage>(phases));
        }
        else {
          Assert::Fail(L"Tracker should remain");
        }
      }

      TestEntityRegistry mRegistry;
      TestEntity mEntity;
      std::shared_ptr<TestScheduler> mScheduler = std::make_shared<TestScheduler>(SchedulerConfig{});
    };

    template<class... Systems>
    static std::vector<std::shared_ptr<ISystem<TestEntity>>> buildSystems() {
      return { makeSystem("test", &Systems::tick)... };
    }

    TEST_METHOD(EmptyAppContext_Update_NothingHappens) {
      RegistryWithPhaseTracker registry;
      TestAppContext context(registry.mScheduler);
      context.buildExecutionGraph();

      const bool updated = context.update(registry.mRegistry);

      registry.assertPhasesTicked({});
      Assert::IsFalse(updated);
    }

    TEST_METHOD(AppContextSinglePhase_NoTicks_NotTicked) {
      RegistryWithPhaseTracker registry;
      TestAppContext context(registry.mScheduler);
      context.registerUpdatePhase(TestStage::A, buildSystems<PhaseTrackerSystem<TestStage::A>>(), 1000);
      context.buildExecutionGraph();

      const bool updated = context.update(registry.mRegistry);

      registry.assertPhasesTicked({});
      Assert::IsFalse(updated);
    }

    TEST_METHOD(AppContextSinglePhase_TwoTicksWithIncrement_TickedTwice) {
      RegistryWithPhaseTracker registry;
      TestAppContext context(registry.mScheduler);
      context.registerUpdatePhase(TestStage::A, buildSystems<PhaseTrackerSystem<TestStage::A>>(), 1000);
      context.buildExecutionGraph();
      setTimeMS(2);
      timeIncrement = 1;

      const bool updated = context.update(registry.mRegistry);

      timeIncrement = 0;
      registry.assertPhasesTicked({ TestStage::A, TestStage::A });
      Assert::IsTrue(updated);
    }

    TEST_METHOD(AppContextSinglePhase_HasTicks_Ticked) {
      RegistryWithPhaseTracker registry;
      TestAppContext context(registry.mScheduler);
      context.registerUpdatePhase(TestStage::A, buildSystems<PhaseTrackerSystem<TestStage::A>>(), 1000);
      context.buildExecutionGraph();
      setTimeMS(1);

      const bool updated = context.update(registry.mRegistry);

      registry.assertPhasesTicked({ TestStage::A });
      Assert::IsTrue(updated);
    }

    TEST_METHOD(AppContextSinglePhase_HadTwoTicks_NotTickedAnymore) {
      RegistryWithPhaseTracker registry;
      TestAppContext context(registry.mScheduler);
      context.registerUpdatePhase(TestStage::A, buildSystems<PhaseTrackerSystem<TestStage::A>>(), 1000);
      context.buildExecutionGraph();
      setTimeMS(2);
      context.update(registry.mRegistry);

      const bool updated = context.update(registry.mRegistry);

      registry.assertPhasesTicked({ TestStage::A, TestStage::A });
      Assert::IsFalse(updated);
    }

    TEST_METHOD(AppContextTwoPhases_BothHaveTicks_BothTicked) {
      RegistryWithPhaseTracker registry;
      TestAppContext context(registry.mScheduler);
      context.registerUpdatePhase(TestStage::A, buildSystems<PhaseTrackerSystem<TestStage::A>>(), 1000);
      context.registerUpdatePhase(TestStage::B, buildSystems<PhaseTrackerSystem<TestStage::B>>(), 1000);
      context.buildExecutionGraph();
      setTimeMS(1);

      const bool updated = context.update(registry.mRegistry);

      registry.assertPhasesTicked({ TestStage::A, TestStage::B });
      Assert::IsTrue(updated);
    }

    TEST_METHOD(AppContextTwoPhases_OneHasTicks_OneTicked) {
      RegistryWithPhaseTracker registry;
      TestAppContext context(registry.mScheduler);
      context.registerUpdatePhase(TestStage::A, buildSystems<PhaseTrackerSystem<TestStage::A>>(), 100);
      context.registerUpdatePhase(TestStage::B, buildSystems<PhaseTrackerSystem<TestStage::B>>(), 1000);
      context.buildExecutionGraph();
      setTimeMS(1);

      const bool updated = context.update(registry.mRegistry);

      registry.assertPhasesTicked({ TestStage::B });
      Assert::IsTrue(updated);
    }

    TEST_METHOD(AppContextThreePhases_MiddleSkipped_DependenciesObeyed) {
      RegistryWithPhaseTracker registry;
      TestAppContext context(registry.mScheduler);
      //Add a slight delay to coax out a bug where C skips ahead
      context.registerUpdatePhase(TestStage::A, buildSystems<PhaseTrackerSystem<TestStage::A, 10>>(), 1000);
      context.registerUpdatePhase(TestStage::B, buildSystems<PhaseTrackerSystem<TestStage::B>>(), 100);
      context.registerUpdatePhase(TestStage::C, buildSystems<PhaseTrackerSystem<TestStage::C>>(), 1000);
      context.buildExecutionGraph();
      setTimeMS(1);

      const bool updated = context.update(registry.mRegistry);

      registry.assertPhasesTicked({ TestStage::A, TestStage::C });
      Assert::IsTrue(updated);
    }

    TEST_METHOD(AppContextThreePhases_RegisteredOutOfOrder_TickedInOrder) {
      RegistryWithPhaseTracker registry;
      TestAppContext context(registry.mScheduler);
      //Add a slight delay to coax out a bug where C skips ahead
      context.registerUpdatePhase(TestStage::C, buildSystems<PhaseTrackerSystem<TestStage::C>>(), 1000);
      context.registerUpdatePhase(TestStage::B, buildSystems<PhaseTrackerSystem<TestStage::B>>(), 100);
      context.registerUpdatePhase(TestStage::A, buildSystems<PhaseTrackerSystem<TestStage::A, 10>>(), 1000);
      context.buildExecutionGraph();
      setTimeMS(1);

      const bool updated = context.update(registry.mRegistry);

      registry.assertPhasesTicked({ TestStage::A, TestStage::C });
      Assert::IsTrue(updated);
    }

    TEST_METHOD(AppContextThreePhases_PhaseReplaced_IsReplaced) {
      RegistryWithPhaseTracker registry;
      TestAppContext context(registry.mScheduler);
      //Add a slight delay to coax out a bug where C skips ahead
      context.registerUpdatePhase(TestStage::A, buildSystems<PhaseTrackerSystem<TestStage::A, 10>>(), 1000);
      context.registerUpdatePhase(TestStage::B, buildSystems<PhaseTrackerSystem<TestStage::A>>(), 1000);
      context.registerUpdatePhase(TestStage::C, buildSystems<PhaseTrackerSystem<TestStage::C>>(), 1000);
      context.registerUpdatePhase(TestStage::B, buildSystems<PhaseTrackerSystem<TestStage::B>>(), 1000);
      context.buildExecutionGraph();
      setTimeMS(1);

      const bool updated = context.update(registry.mRegistry);

      registry.assertPhasesTicked({ TestStage::A, TestStage::B, TestStage::C });
      Assert::IsTrue(updated);
    }

    //Test to ensure that the systems injected to process command buffers properly gather all thread local command buffers
    TEST_METHOD(MultipleCommandBuffers_CreateEntities_AllCreated) {
      RegistryWithPhaseTracker registry;
      TestAppContext context(registry.mScheduler);
      std::vector<TestSystem> systems;
      using CommandT = EntityCommandBuffer<TestEntity, int, float>;
      systems.push_back(ecx::makeSystem("", [](SystemContext<TestEntity, CommandT>& context) {
        CommandT cmd = context.get<CommandT>();
        cmd.createAndGetEntityWithComponents<int>();
      }, uint64_t(1)));
      systems.push_back(ecx::makeSystem("", [](SystemContext<TestEntity, CommandT>& context) {
        context.get<CommandT>().createAndGetEntityWithComponents<float>();
      }, uint64_t(2)));
      context.registerUpdatePhase(TestStage::A, std::move(systems), 1000);
      context.buildExecutionGraph();
      context.initialize(registry.mRegistry);
      setTimeMS(1);

      context.update(registry.mRegistry);

      Assert::AreEqual(size_t(1), registry.mRegistry.size<int>(), L"Thread 1 command should have been processed");
      Assert::AreEqual(size_t(1), registry.mRegistry.size<float>(), L"Thread 2 command should have been processed");
    }
  };
}