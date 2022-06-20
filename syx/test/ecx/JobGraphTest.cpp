#include "Precompile.h"
#include "CppUnitTest.h"

#include "JobGraph.h"
#include "JobInfo.h"
#include "LinearEntityRegistry.h"
#include "LinearView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(JobGraphTest) {
    using TestEntity = LinearEntity;
    using TestEntityRegistry = EntityRegistry<TestEntity>;
    using TestSystem = ISystem<TestEntity>;
    using TestEntityFactory = EntityFactory<TestEntity>;
    template<class... Args>
    using TestEntityModifier = EntityModifier<TestEntity, Args...>;
    template<class... Args>
    using TestView = View<TestEntity, Args...>;
    template<class... Args>
    using TestCommandBuffer = EntityCommandBuffer<TestEntity, Args...>;

    using SystemList = std::vector<std::shared_ptr<TestSystem>>;

    bool tryRunInOrder(std::vector<std::shared_ptr<TestSystem>> systems, std::vector<std::shared_ptr<TestSystem>> order) {
      auto root = JobGraph::build(systems);
      JobGraph::resetDependencies(*root);
      ThreadLocalContext tlc;
      TestEntityRegistry registry;
      //Normally AppContext::initialize would do this
      tlc.emplace<ecx::CommandBuffer<TestEntity>>(registry);
      std::vector<std::shared_ptr<JobInfo<TestEntity>>> work;
      auto queueToThread = [&work](size_t, std::shared_ptr<JobInfo<TestEntity>> job) {
        work.push_back(std::move(job));
      };

      work.push_back(root);
      while(!order.empty() && !work.empty()) {
        //First try to find the next item in the desired order
        auto findDesired = std::find_if(work.begin(), work.end(), [next(order.front())](const auto& job) { return job->mSystem == next; });
        if(findDesired != work.end()) {
          order.erase(order.begin());
          auto job = *findDesired;
          work.erase(findDesired);

          JobGraph::runSystems(registry, tlc, *job, work, queueToThread);
          continue;
        }

        //If desired work can't be found, pick an arbitrary one that wouldn't violate the desired order
        std::shared_ptr<JobInfo<TestEntity>> todo;
        for(auto it = work.begin(); it != work.end(); ++it) {
          auto foundInOrder = std::find_if(order.begin(), order.end(), [&it](const auto& system) {
            return (*it)->mSystem == system;
          });
          //If this wasn't in the order list, it can be taken as the current work item because it wouldn't violate the order
          if(foundInOrder == order.end()) {
            todo = *it;
            work.erase(it);
            break;
          }
        }

        if(todo) {
          JobGraph::runSystems(registry, tlc, *todo, work, queueToThread);
        }
        else {
          //No work could be found meaning the desired order is impossible
          return false;
        }
      }
      return order.empty();
    }

    struct Permuter {
      bool operator<(const Permuter& rhs) {
        return mOrder < rhs.mOrder;
      }

      static std::vector<Permuter> build(const std::vector<std::shared_ptr<TestSystem>>& systems) {
        std::vector<Permuter> result;
        result.reserve(systems.size());
        for(size_t i = 0; i < systems.size(); ++i) {
          result.push_back({ systems[i], i });
        }
        return result;
      }

      static std::vector<std::shared_ptr<TestSystem>> toSystemList(std::vector<Permuter> systems) {
        std::vector<std::shared_ptr<TestSystem>> result;
        std::transform(systems.begin(), systems.end(), std::back_inserter(result), [](auto& p) { return p.mSystem; });
        return result;
      }

      std::shared_ptr<TestSystem> mSystem;
      size_t mOrder = 0;
    };

    template<class... ContextArgs>
    std::shared_ptr<TestSystem> createSystem(std::optional<size_t> threadRequirement = {}) {
      return makeSystem("test", [](SystemContext<TestEntity, ContextArgs...>&) {}, threadRequirement);
    };

    auto buildGraph(std::initializer_list<std::shared_ptr<TestSystem>> systems) {
      SystemList s(systems);
      return JobGraph::build(s);
    }

    void testSystemsExpectParallel(std::initializer_list<std::shared_ptr<TestSystem>> systems) {
      auto permuter = Permuter::build(systems);
      std::vector<std::shared_ptr<TestSystem>> systemList(systems);
      do {
        Assert::IsTrue(tryRunInOrder(systemList, Permuter::toSystemList(permuter)));
      }
      while(std::next_permutation(permuter.begin(), permuter.end()));
    }

    void testSystemsExpectSequential(std::initializer_list<std::shared_ptr<TestSystem>> systems) {
      auto permuter = Permuter::build(systems);
      std::vector<std::shared_ptr<TestSystem>> systemList(systems);
      bool originalOrder = true;
      do {
        if(originalOrder) {
          Assert::IsTrue(tryRunInOrder(systemList, Permuter::toSystemList(permuter)), L"Original order should work");
          originalOrder = false;
        }
        else {
          Assert::IsFalse(tryRunInOrder(systemList, Permuter::toSystemList(permuter)), L"No other orders should work");
        }
      }
      while(std::next_permutation(permuter.begin(), permuter.end()));
    }

    TEST_METHOD(JobGraph_NoSystems_EmptyRoot) {
      SystemList systems;
      auto job = JobGraph::build(systems);

      Assert::IsTrue(job != nullptr);
      Assert::AreEqual(job->mTotalDependencies, uint32_t(0));
      Assert::AreEqual(size_t(1), job->mDependents.size(), L"Should only contain command buffer processing system");
    }

    TEST_METHOD(JobGraph_TwoExistenceChecks_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestView<Include<int>>>(),
        createSystem<TestView<Exclude<int>>>()
      });
    }

    TEST_METHOD(JobGraph_ExistenceAndRead_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestView<Include<int>>>(),
        createSystem<TestView<Read<int>>>()
      });
    }

    TEST_METHOD(JobGraph_ExistenceAndWrite_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestView<Exclude<int>>>(),
        createSystem<TestView<Write<int>>>()
      });
    }

    TEST_METHOD(JobGraph_ExistenceAndCreateUnrelated_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestView<Include<int>>>(),
        createSystem<TestEntityModifier<char>>()
      });
    }

    TEST_METHOD(JobGraph_ExistenceAndCreateSame_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestView<Include<int>>>(),
        createSystem<TestEntityModifier<int>>()
      });
    }

    TEST_METHOD(JobGraph_ExistenceAndEntityFactory_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestView<Include<int>>>(),
        createSystem<TestEntityFactory>()
      });
    }

    TEST_METHOD(JobGraph_TwoUnrelatedReads_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestView<Read<int>>>(),
        createSystem<TestView<Read<char>>>(),
      });
    }

    TEST_METHOD(JobGraph_TwoCommonReads_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestView<Read<int>>>(),
        createSystem<TestView<Read<int>>>(),
      });
    }

    TEST_METHOD(JobGraph_ReadAndUnrelatedWrite_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestView<Read<int>>>(),
        createSystem<TestView<Write<char>>>()
      });
    }

    TEST_METHOD(JobGraph_ReadAndSameWrite_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestView<Read<int>>>(),
        createSystem<TestView<Write<int>>>()
      });
    }

    TEST_METHOD(JobGraph_ReadAndUnrelatedModifier_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestView<Read<int>>>(),
        createSystem<TestEntityModifier<char>>(),
      });
    }

    TEST_METHOD(JobGraph_ReadAndSameModifier_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestView<Read<int>>>(),
        createSystem<TestEntityModifier<int>>()
      });
    }

    TEST_METHOD(JobGraph_ReadAndEntityFactory_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestView<Read<int>>>(),
        createSystem<TestEntityFactory>()
      });
    }

    TEST_METHOD(JobGraph_UnrelatedWrites_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestView<Write<int>>>(),
        createSystem<TestView<Write<char>>>()
      });
    }

    TEST_METHOD(JobGraph_SameWrites_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestView<Write<int>>>(),
        createSystem<TestView<Write<int>>>()
      });
    }

    TEST_METHOD(JobGraph_WriteAndUnrelatedModifier_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestView<Write<int>>>(),
        createSystem<TestEntityModifier<char>>(),
      });
    }

    TEST_METHOD(JobGraph_WriteAndSameModifier_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestView<Write<int>>>(),
        createSystem<TestEntityModifier<int>>()
      });
    }

    TEST_METHOD(JobGraph_WriteAndEntityFactory_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestView<Write<int>>>(),
        createSystem<TestEntityFactory>()
      });
    }

    TEST_METHOD(JobGraph_UnrelatedModifiers_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestEntityModifier<int>>(),
        createSystem<TestEntityModifier<short>>()
      });
    }

    TEST_METHOD(JobGraph_SameEntityModifiers_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestEntityModifier<int>>(),
        createSystem<TestEntityModifier<int>>()
      });
    }

    TEST_METHOD(JobGraph_ModifierAndFactory_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestEntityModifier<int>>(),
        createSystem<TestEntityFactory>()
      });
    }

    TEST_METHOD(JobGraph_TwoEntityFactories_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestEntityFactory>(),
        createSystem<TestEntityFactory>()
      });
    }

    TEST_METHOD(JobGraph_AllConflictingOperations_Sequential) {
      testSystemsExpectSequential({
        createSystem<TestView<Read<int>>>(),
        createSystem<TestView<Write<int>>>(),
        createSystem<TestEntityModifier<int>>(),
        createSystem<TestView<Write<int>>>(),
        createSystem<TestView<Read<int>>>(),
        createSystem<TestEntityFactory>(),
        createSystem<TestView<Read<char>>>()
      });
    }

    TEST_METHOD(JobGraph_AllNonConflictingOperations_Parallel) {
      testSystemsExpectParallel({
        createSystem<TestView<Read<int>>>(),
        createSystem<TestView<Write<char>>>(),
        createSystem<TestView<Include<int>>>(),
        createSystem<TestView<Include<std::string>>>(),
        createSystem<TestView<Exclude<short>>>(),
        createSystem<TestEntityModifier<uint64_t>>()
      });
    }

    //This test illustrates the current behavior, which is more aggressive than necessary.
    //Extra graph traversal could be done to remove redundant edges
    TEST_METHOD(JobGraph_ComplicatedGraph_ExpectedDependencies) {
      auto a = createSystem<TestEntityFactory>();
      auto b = createSystem<TestView<Read<int>>>();
      auto c = createSystem<TestView<Write<char>>>();
      auto d = createSystem<TestView<Read<int>, Read<char>, Write<std::string>>>();
      auto e = createSystem<TestView<Read<int>>>();
      auto f = createSystem<TestView<Read<int>, Read<std::string>>>();
      auto g = createSystem<TestView<Include<int>, Exclude<char>, Include<std::string>>>();
      auto h = createSystem<TestEntityModifier<int, char>>();
      auto i = createSystem<TestView<Write<std::string>>>();
      auto j = createSystem<TestEntityFactory>();
      auto k = createSystem<TestView<Read<int>>>();

      auto root = buildGraph({ a, b, c, d, e, f, g, h, i, j, k });

      // Validate root
      Assert::AreEqual(size_t(1), root->mDependents.size(), L"Only TestEntityFactory should be in the root", LINE_INFO());

      //Validate a
      auto node = root->mDependents.front();
      Assert::IsTrue(node->mSystem.get() == a.get());
      // b, c, e, g, h, j
      Assert::AreEqual(size_t(9), node->mDependents.size());
      Assert::IsTrue(b.get() == node->mDependents[0]->mSystem.get());
      Assert::IsTrue(c.get() == node->mDependents[1]->mSystem.get());
      Assert::IsTrue(d.get() == node->mDependents[2]->mSystem.get());
      Assert::IsTrue(e.get() == node->mDependents[3]->mSystem.get());
      Assert::IsTrue(f.get() == node->mDependents[4]->mSystem.get());
      Assert::IsTrue(g.get() == node->mDependents[5]->mSystem.get());
      Assert::IsTrue(h.get() == node->mDependents[6]->mSystem.get());
      Assert::IsTrue(i.get() == node->mDependents[7]->mSystem.get());
      Assert::IsTrue(j.get() == node->mDependents[8]->mSystem.get());

      //Validate b
      auto bNode = node->mDependents[0];
      Assert::IsTrue(b.get() == bNode->mSystem.get());
      // h
      Assert::AreEqual(size_t(1), bNode->mDependents.size());
      Assert::IsTrue(h.get() == bNode->mDependents[0]->mSystem.get());

      //Validate c
      auto cNode = node->mDependents[1];
      Assert::IsTrue(c.get() == cNode->mSystem.get());
      // d, h
      Assert::AreEqual(size_t(2), cNode->mDependents.size());
      Assert::IsTrue(d.get() == cNode->mDependents[0]->mSystem.get());
      Assert::IsTrue(h.get() == cNode->mDependents[1]->mSystem.get());

      //Validate d
      auto dNode = cNode->mDependents[0];
      // f, h, i, j
      Assert::AreEqual(size_t(4), dNode->mDependents.size());
      Assert::IsTrue(f.get() == dNode->mDependents[0]->mSystem.get());
      Assert::IsTrue(h.get() == dNode->mDependents[1]->mSystem.get());
      Assert::IsTrue(i.get() == dNode->mDependents[2]->mSystem.get());
      Assert::IsTrue(j.get() == dNode->mDependents[3]->mSystem.get());

      //Validate e
      auto eNode = node->mDependents[3];
      // h
      Assert::AreEqual(size_t(1), eNode->mDependents.size());
      Assert::IsTrue(h.get() == eNode->mDependents[0]->mSystem.get());

      //Validate f
      auto fNode = dNode->mDependents[0];
      // h, i, j
      Assert::AreEqual(size_t(3), fNode->mDependents.size());
      Assert::IsTrue(h.get() == fNode->mDependents[0]->mSystem.get());
      Assert::IsTrue(i.get() == fNode->mDependents[1]->mSystem.get());
      Assert::IsTrue(j.get() == fNode->mDependents[2]->mSystem.get());

      //Validate g
      auto gNode = node->mDependents[5];
      // h, j
      Assert::AreEqual(size_t(2), gNode->mDependents.size());
      Assert::IsTrue(h.get() == gNode->mDependents[0]->mSystem.get());
      Assert::IsTrue(j.get() == gNode->mDependents[1]->mSystem.get());

      //Validate h
      auto hNode = node->mDependents[6];
      // j
      Assert::AreEqual(size_t(1), hNode->mDependents.size());
      Assert::IsTrue(j.get() == hNode->mDependents[0]->mSystem.get());

      //Validate i
      auto iNode = dNode->mDependents[2];
      // j
      Assert::AreEqual(size_t(1), iNode->mDependents.size());
      Assert::IsTrue(j.get() == iNode->mDependents[0]->mSystem.get());

      //Validate j
      auto jNode = node->mDependents[8];
      // k
      Assert::AreEqual(size_t(2), jNode->mDependents.size(), L"Should point at J and a final command buffer processor system");
      Assert::IsTrue(k.get() == jNode->mDependents[0]->mSystem.get());

      //Validate k
      Assert::AreEqual(size_t(1), jNode->mDependents[0]->mDependents.size(), L"Should only depend on final command buffer processor system");
    }

    TEST_METHOD(JobGraph_ResetSingleDependency_IsReset) {
      auto r = createSystem<TestView<Write<int>>>();
      auto a = createSystem<TestView<Read<int>>>();
      auto root = buildGraph({ r, a });

      JobGraph::resetDependencies(*root);

      Assert::AreEqual(root->mDependents[0]->mTotalDependencies, root->mDependents[0]->mDependencies.load());
    }

    TEST_METHOD(JobGraph_ResetDependencyTree_IsReset) {
      auto r = createSystem<TestView<Write<int>>>();
      auto a = createSystem<TestView<Write<int>>>();
      auto b = createSystem<TestView<Write<int>>>();
      auto root = buildGraph({ r, a, b });

      JobGraph::resetDependencies(*root);

      auto aNode = root->mDependents[0];
      Assert::AreEqual(aNode->mTotalDependencies, aNode->mDependencies.load());
      auto bNode = aNode->mDependents[0];
      Assert::AreEqual(bNode->mTotalDependencies, bNode->mDependencies.load());
    }

    TEST_METHOD(JobGraph_RunSystems_AreRunInOrder) {
      TestEntityRegistry registry;
      std::deque<std::shared_ptr<JobInfo<TestEntity>>> jobs;
      std::vector<TestSystem*> resultOrder;
      auto a = createSystem<TestView<Write<int>>>();
      auto b = createSystem<TestView<Read<char>>>();
      auto c = createSystem<TestView<Write<char>>>();
      auto d = createSystem<TestView<Read<char>, Read<int>>>();
      auto e = createSystem<TestEntityModifier<char>>();
      auto f = createSystem<TestEntityFactory>();
      auto g = createSystem<TestView<Read<int>>>();
      auto root = buildGraph({ a, b, c, d, e, f, g });
      JobGraph::resetDependencies(*root);
      auto expectedSizes = std::invoke([] {
        std::queue<size_t> sizes;
        auto temp = { 1, //root
          2, // a, b,
          1, // b
          1, // c,
          1, // d
          1, // e
          1, // f
          1, // g
        };
        for(auto&& s : temp) {
          sizes.push(static_cast<size_t>(s));
        }
        return sizes;
      });
      jobs.push_back(root);
      while(!jobs.empty() && !expectedSizes.empty()) {
        Assert::AreEqual(expectedSizes.front(), jobs.size());
        expectedSizes.pop();
        auto job = jobs.front();
        jobs.pop_front();

        if(job->mSystem) {
          resultOrder.push_back(job->mSystem.get());
        }

        ThreadLocalContext context;
        JobGraph::runSystems(registry, context, *job, jobs, [](auto&&...) { Assert::Fail(); });
      }

      auto expected = std::vector<TestSystem*>{ a.get(), b.get(), c.get(), d.get(), e.get(), f.get(), g.get() };
      Assert::IsTrue(expected == resultOrder);
    }

    TEST_METHOD(JobGraph_RunSystemWithThreadConstraint_IsQueuedToThread) {
      TestEntityRegistry registry;
      std::deque<std::shared_ptr<JobInfo<TestEntity>>> jobs;
      std::vector<TestSystem*> resultOrder;
      auto a = createSystem<TestView<Write<int>>>(std::make_optional(size_t(1)));
      auto root = buildGraph({ a });
      JobGraph::resetDependencies(*root);
      int wasQueued = 0;

      ThreadLocalContext context;
      JobGraph::runSystems(registry, context, *root, jobs, [&wasQueued](size_t index, std::shared_ptr<JobInfo<TestEntity>> job) {
        Assert::AreEqual(size_t(1), index);
        ++wasQueued;
      });

      Assert::AreEqual(1, wasQueued, L"Should have been queed once", LINE_INFO());
    }

    TEST_METHOD(JobGraph_ReadWriteFactoryPermutations_OrdersWork) {
      auto a = createSystem<TestView<Read<int>>>();
      auto b = createSystem<TestView<Read<int>>>();
      auto c = createSystem<TestView<Read<double>>>();
      auto d = createSystem<TestView<Write<double>>>();
      auto e = createSystem<TestEntityFactory>();
      std::vector systems{ a, b, c, d, e };
      Assert::IsTrue(tryRunInOrder(systems, { a, b, c, d, e }));
      Assert::IsTrue(tryRunInOrder(systems, { b, a, c, d, e }));
      Assert::IsTrue(tryRunInOrder(systems, { a, c, b, d, e }));
      Assert::IsTrue(tryRunInOrder(systems, { c, d, a, b, e }));
      Assert::IsFalse(tryRunInOrder(systems, { d, c, a, b, e }), L"Shouldn't be able to put a write before a read");
      Assert::IsTrue(tryRunInOrder(systems, { c, b, a, d, e }));
      Assert::IsFalse(tryRunInOrder(systems, { a, b, c, e, d }), L"Shouldn't be possible to run entity factory before anything else");
      Assert::IsFalse(tryRunInOrder(systems, { a, b, e, c, d }));
      Assert::IsFalse(tryRunInOrder(systems, { a, e, b, c, d }));
      Assert::IsFalse(tryRunInOrder(systems, { e, a, b, c, d }));
    }

    TEST_METHOD(JobGraph_CommandBufferOrder_IsCorrect) {
      auto a = createSystem<TestView<Read<int>>>();
      auto b = createSystem<TestCommandBuffer<int>>();
      auto c = createSystem<TestView<Read<double>>>();
      auto d = createSystem<TestView<Write<int>>>();
      std::vector systems{ a, b, c, d };

      Assert::IsTrue(tryRunInOrder(systems, { a, b, c, d }));
      Assert::IsTrue(tryRunInOrder(systems, { a, c, b, d }));
      Assert::IsFalse(tryRunInOrder(systems, { b, a, c, d }));
      Assert::IsFalse(tryRunInOrder(systems, { a, c, d, b }));
    }

    TEST_METHOD(JobGraph_CommandBufferRemoveOrder_IsCorrect) {
      auto a = createSystem<TestView<Read<int>>>();
      auto b = createSystem<TestCommandBuffer<ecx::EntityDestroyTag>>();
      auto c = createSystem<TestView<Read<double>>>();
      std::vector systems{ a, b, c };

      Assert::IsTrue(tryRunInOrder(systems, { a, b, c }));
      Assert::IsFalse(tryRunInOrder(systems, { b, a, c }));
      Assert::IsFalse(tryRunInOrder(systems, { a, c, b }));
    }

    TEST_METHOD(JobGraph_CommandBufferAdd_VisibleByNextSystem) {
      std::shared_ptr<TestSystem> a = ecx::makeSystem("", [](SystemContext<TestEntity, TestCommandBuffer<int>>& ctx) {
        auto buffer = ctx.get<TestCommandBuffer<int>>();
        auto&& [entity, i] = buffer.createAndGetEntityWithComponents<int>();
        *i = 7;
      });
      std::shared_ptr<TestSystem> b = ecx::makeSystem("", [](SystemContext<TestEntity, TestView<Read<int>>>& ctx) {
        auto& view = ctx.get<TestView<Read<int>>>();
        Assert::IsTrue(view.begin() != view.end());
        Assert::AreEqual(7, (*view.begin()).get<const int>());
      });
      std::vector systems{ a, b };

      Assert::IsTrue(tryRunInOrder(systems, { a, b }));
    }

    TEST_METHOD(JobGraph_CommandBufferAddRemove_VisibleByNextSystem) {
      TestEntity e;
      std::shared_ptr<TestSystem> a = ecx::makeSystem("", [&e](SystemContext<TestEntity, TestCommandBuffer<int>>& ctx) {
        auto buffer = ctx.get<TestCommandBuffer<int>>();
        auto&& [entity, i] = buffer.createAndGetEntityWithComponents<int>();
        e = entity;
        *i = 7;
      });
      std::shared_ptr<TestSystem> b = ecx::makeSystem("", [](SystemContext<TestEntity, TestView<Read<int>>>& ctx) {
        auto& view = ctx.get<TestView<Read<int>>>();
        Assert::IsTrue(view.begin() != view.end());
        Assert::AreEqual(7, (*view.begin()).get<const int>());
      });
      std::shared_ptr<TestSystem> c = ecx::makeSystem("", [&e](SystemContext<TestEntity, TestCommandBuffer<ecx::EntityDestroyTag>>& ctx) {
        auto buffer = ctx.get<TestCommandBuffer<ecx::EntityDestroyTag>>();
        buffer.destroyEntity(e);
      });
      std::shared_ptr<TestSystem> d = ecx::makeSystem("", [](SystemContext<TestEntity, TestView<Read<int>>>& ctx) {
        auto& view = ctx.get<TestView<Read<int>>>();
        Assert::IsTrue(view.begin() == view.end());
      });
      std::vector systems{ a, b, c, d };

      Assert::IsTrue(tryRunInOrder(systems, { a, b, c, d }));
    }
  };
}