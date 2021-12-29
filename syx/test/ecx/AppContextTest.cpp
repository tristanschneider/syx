#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppContext.h"
#include "Scheduler.h"

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

    using TestEntity = uint32_t;
    using TestEntityRegistry = EntityRegistry<TestEntity>;
    using TestEntityFactory = EntityFactory<TestEntity>;
    using TestScheduler = Scheduler<TestEntity, LockQueue>;
    using TestTimer = Timer<100, &mockGetTime>;
    template<class... Rest>
    using TestView = View<TestEntity, Rest...>;

    enum class TestStage {
      A,
      B,
      C,
    };

    using TestAppContext = AppContext<TestScheduler, TestTimer, TestStage, TestEntity>;

    TEST_METHOD(Timer_DefaultConstruct_NoTicks) {
      TestTimer timer(1000);

      Assert::AreEqual(size_t(0), timer.redeemTicks());
    }

    TEST_METHOD(Timer_LessTimeElapsed_NoTicks) {
      setTimeMS(0);
      TestTimer timer(100);
      setTimeMS(5);

      Assert::AreEqual(size_t(0), timer.redeemTicks());
    }

    TEST_METHOD(Timer_TickTimeElapsed_OneTick) {
      setTimeMS(0);
      TestTimer timer(100);
      setTimeMS(10);

      Assert::AreEqual(size_t(1), timer.redeemTicks());
      Assert::AreEqual(size_t(0), timer.redeemTicks());
    }

    TEST_METHOD(Timer_TwoTicksElapsed_TwoTicks) {
      setTimeMS(0);
      TestTimer timer(100);
      setTimeMS(20);

      Assert::AreEqual(size_t(2), timer.redeemTicks());
      Assert::AreEqual(size_t(0), timer.redeemTicks());
    }

    TEST_METHOD(Timer_TickWithRemainder_RedeemedOnSecondTick) {
      setTimeMS(0);
      TestTimer timer(100);
      setTimeMS(15);
      timer.redeemTicks();
      setTimeMS(20);

      Assert::AreEqual(size_t(1), timer.redeemTicks());
      Assert::AreEqual(size_t(0), timer.redeemTicks());
    }

    TEST_METHOD(Timer_ZeroTarget_AlwaysOneTick) {
      TestTimer timer(0);

      Assert::AreEqual(size_t(1), timer.redeemTicks());
      Assert::AreEqual(size_t(1), timer.redeemTicks());
    }

    TEST_METHOD(Timer_OverCap_RedeemedOnNextTick) {
      setTimeMS(0);
      TestTimer timer(1000);
      setTimeMS(101);

      Assert::AreEqual(size_t(100), timer.redeemTicks());
      Assert::AreEqual(size_t(1), timer.redeemTicks());
      Assert::AreEqual(size_t(0), timer.redeemTicks());
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
        mEntity = mRegistry.createEntity();
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

  };
}