#include "Precompile.h"
#include "CppUnitTest.h"

#include "JobGraph.h"
#include "JobInfo.h"
#include "Scheduler.h"
#include "SchedulerComponent.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(SchedulerTest) {
    using TestEntity = uint32_t;
    using TestEntityRegistry = EntityRegistry<TestEntity>;
    using TestSystem = ISystem<TestEntity>;
    using TestEntityFactory = EntityFactory<TestEntity>;
    template<class... Args>
    using TestEntityModifier = EntityModifier<TestEntity, Args...>;
    template<class... Args>
    using TestView = View<TestEntity, Args...>;
    using SystemList = std::vector<std::shared_ptr<TestSystem>>;
    template<class... Args>
    using TestSystemContext = SystemContext<TestEntity, Args...>;
    using TestScheduler = DefaultSchedulerT<TestEntity>;
    using TestSchedulerComponent = SchedulerComponent<TestScheduler>;

    std::shared_ptr<size_t> addLinearJobsWithTasks(SystemList& systems, size_t expectedSequence, std::shared_ptr<size_t> curSequence, std::shared_ptr<std::atomic_size_t> completed, size_t count) {
      std::shared_ptr<size_t> sequence = curSequence ? curSequence : std::make_shared<size_t>(size_t(0));
      for(size_t i = expectedSequence; i < count + expectedSequence; ++i) {
        systems.push_back(makeSystem("test", [sequence, i, completed](TestSystemContext<TestView<Write<int>>, TestSchedulerComponent::ViewT>& context) {
          Assert::AreEqual(i, *sequence, L"Systems should have executed in sequence");

          auto executor = TestSchedulerComponent::createExecutorFromContext(context);
          auto workCounter = std::make_shared<std::atomic_int32_t>(0);
          for(size_t j = 0; j < 100; ++j) {
            executor.queueTask([workCounter] {
              ++(*workCounter);
            });
          }

          executor.sync();

          Assert::AreEqual(100, workCounter->load());

          ++(*sequence);
          ++(*completed);
        }));
      }
      return sequence;
    }

    std::shared_ptr<size_t> addLinearJobs(SystemList& systems, size_t expectedSequence, std::shared_ptr<size_t> curSequence, std::shared_ptr<std::atomic_size_t> completed, size_t count) {
      std::shared_ptr<size_t> sequence = curSequence ? curSequence : std::make_shared<size_t>(size_t(0));
      for(size_t i = expectedSequence; i < count + expectedSequence; ++i) {
        systems.push_back(makeSystem("test", [sequence, i, completed](TestSystemContext<TestView<Write<int>>>&) {
          Assert::AreEqual(i, *sequence, L"Systems should have executed in sequence");
          ++(*sequence);
          ++(*completed);
        }));
      }
      return sequence;
    }

    std::shared_ptr<size_t> addParallelJobs(SystemList& systems, size_t expectedSequence, std::shared_ptr<size_t> curSequence, std::shared_ptr<std::atomic_size_t> completed, size_t count) {
      std::shared_ptr<size_t> sequence = curSequence ? curSequence : std::make_shared<size_t>(size_t(0));
      for(size_t i = 0; i < count; ++i) {
        systems.push_back(makeSystem("test", [seq(expectedSequence), curSequence(sequence), completed](TestSystemContext<TestView<Read<int>>>&) {
          Assert::AreEqual(seq, *curSequence, L"All parallel systems should have executed before sequence advanced");
          ++(*completed);
        }));
      }
      return sequence;
    }

    std::shared_ptr<size_t> addParallelJobsWithTasks(SystemList& systems, size_t expectedSequence, std::shared_ptr<size_t> curSequence, std::shared_ptr<std::atomic_size_t> completed, size_t count) {
      std::shared_ptr<size_t> sequence = curSequence ? curSequence : std::make_shared<size_t>(size_t(0));
      for(size_t i = 0; i < count; ++i) {
        systems.push_back(makeSystem("test", [seq(expectedSequence), curSequence(sequence), completed](TestSystemContext<TestView<Read<int>>, TestView<Read<TestSchedulerComponent>>>& context) {
          Assert::AreEqual(seq, *curSequence, L"All parallel systems should have executed before sequence advanced");

          auto executor = TestSchedulerComponent::createExecutorFromContext(context);
          auto workCounter = std::make_shared<std::atomic_int32_t>(0);
          for(size_t i = 0; i < 100; ++i) {
            executor.queueTask([workCounter] {
              ++(*workCounter);
            });
          }

          executor.sync();

          Assert::AreEqual(100, workCounter->load());

          ++(*completed);
        }));
      }
      return sequence;
    }

    std::shared_ptr<TestScheduler> createScheduler() {
      return std::make_shared<TestScheduler>(SchedulerConfig{});
    }

    std::shared_ptr<std::atomic_size_t> createSystemCount() {
      return std::make_shared<std::atomic_size_t>(0);
    }

    TEST_METHOD(Scheduler_NoSystems_ExitsImmediately) {
      SystemList systems;
      auto scheduler = createScheduler();
      TestEntityRegistry registry;
      auto jobs = JobGraph::build(systems);

      scheduler->execute(registry, *jobs);

      //If the function didn't get stuck in an infinite loop or crash then this passed
    }

    TEST_METHOD(Scheduler_SingleSystem_AllSystemsRun) {
      SystemList systems;
      auto systemCount = createSystemCount();
      addLinearJobs(systems, 0, nullptr, systemCount, 1);
      auto scheduler = createScheduler();
      TestEntityRegistry registry;
      auto jobs = JobGraph::build(systems);

      scheduler->execute(registry, *jobs);

      Assert::AreEqual(size_t(1), systemCount->load(), L"All systems should have executed", LINE_INFO());
    }

    TEST_METHOD(Scheduler_TwoLinearSystems_AllSystemsRun) {
      SystemList systems;
      auto systemCount = createSystemCount();
      addLinearJobs(systems, 0, nullptr, systemCount, 2);
      auto scheduler = createScheduler();
      TestEntityRegistry registry;
      auto jobs = JobGraph::build(systems);

      scheduler->execute(registry, *jobs);

      Assert::AreEqual(size_t(2), systemCount->load(), L"All systems should have executed", LINE_INFO());
    }

    TEST_METHOD(Scheduler_TwoParallelSystems_AllSystemsRun) {
      SystemList systems;
      auto systemCount = createSystemCount();
      addParallelJobs(systems, 0, nullptr, systemCount, 2);
      auto scheduler = createScheduler();
      TestEntityRegistry registry;
      auto jobs = JobGraph::build(systems);

      scheduler->execute(registry, *jobs);

      Assert::AreEqual(size_t(2), systemCount->load(), L"All systems should have executed", LINE_INFO());
    }

    TEST_METHOD(Scheduler_1000ParallelSystems_AllSystemsRun) {
      SystemList systems;
      auto systemCount = createSystemCount();
      addParallelJobs(systems, 0, nullptr, systemCount, 1000);
      auto scheduler = createScheduler();
      TestEntityRegistry registry;
      auto jobs = JobGraph::build(systems);

      scheduler->execute(registry, *jobs);

      Assert::AreEqual(size_t(1000), systemCount->load(), L"All systems should have executed", LINE_INFO());
    }

    TEST_METHOD(Scheduler_1000LinearSystems_AllSystemsRun) {
      SystemList systems;
      auto systemCount = createSystemCount();
      addLinearJobs(systems, 0, nullptr, systemCount, 1000);
      auto scheduler = createScheduler();
      TestEntityRegistry registry;
      auto jobs = JobGraph::build(systems);

      scheduler->execute(registry, *jobs);

      Assert::AreEqual(size_t(1000), systemCount->load(), L"All systems should have executed", LINE_INFO());
    }

    TEST_METHOD(Scheduler_LinearParallelMix_AllSystemsRun) {
      SystemList systems;
      auto systemCount = createSystemCount();
      size_t total = 0;
      auto sequence = addLinearJobs(systems, 0, nullptr, systemCount, 100);
      total += 100;
      addParallelJobs(systems, total, sequence, systemCount, 2);
      addLinearJobs(systems, total, sequence, systemCount, 1);
      total += 1;
      addParallelJobs(systems, total, sequence, systemCount, 1000);
      addLinearJobs(systems, total, sequence, systemCount, 100);
      total += 100;
      total += 2;
      total += 1000;

      auto scheduler = createScheduler();
      TestEntityRegistry registry;
      auto jobs = JobGraph::build(systems);

      scheduler->execute(registry, *jobs);

      Assert::AreEqual(total, systemCount->load(), L"All systems should have executed", LINE_INFO());
    }

    TEST_METHOD(Scheduler_SystemWithTasks_TasksComplete) {
      SystemList systems;
      auto systemCount = createSystemCount();
      addParallelJobsWithTasks(systems, 0, nullptr, systemCount, 1);
      TestEntityRegistry registry;
      auto scheduler = createScheduler();
      addSingletonSchedulerComponent(registry, scheduler);
      auto jobs = JobGraph::build(systems);

      scheduler->execute(registry, *jobs);

      Assert::AreEqual(size_t(1), systemCount->load(), L"All systems should have executed", LINE_INFO());
    }

    TEST_METHOD(Scheduler_ParallelLinearMixWithTasks_TasksComplete) {
      SystemList systems;
      auto systemCount = createSystemCount();
      size_t total = 0;
      auto sequence = addLinearJobs(systems, 0, nullptr, systemCount, 100);
      total += 100;
      addParallelJobsWithTasks(systems, total, sequence, systemCount, 2);
      addLinearJobsWithTasks(systems, total, sequence, systemCount, 1);
      total += 1;
      addParallelJobsWithTasks(systems, total, sequence, systemCount, 1000);
      addLinearJobsWithTasks(systems, total, sequence, systemCount, 100);
      total += 100;
      total += 2;
      total += 1000;
      TestEntityRegistry registry;
      auto scheduler = createScheduler();
      addSingletonSchedulerComponent(registry, scheduler);
      auto jobs = JobGraph::build(systems);

      scheduler->execute(registry, *jobs);

      Assert::AreEqual(total, systemCount->load(), L"All systems should have executed", LINE_INFO());
    }
  };
}