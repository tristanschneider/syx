#include "Precompile.h"
#include "CppUnitTest.h"

#include "JobGraph.h"
#include "JobInfo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ecx {
  TEST_CLASS(JobGraphTest) {
    using TestEntity = uint32_t;
    using TestEntityRegistry = EntityRegistry<TestEntity>;
    using TestSystem = ISystem<TestEntity>;
    using TestEntityFactory = EntityFactory<TestEntity>;
    template<class... Args>
    using TestEntityModifier = EntityModifier<TestEntity, Args...>;
    template<class... Args>
    using TestView = View<TestEntity, Args...>;

    using SystemList = std::vector<std::shared_ptr<TestSystem>>;

    template<class... ContextArgs>
    std::shared_ptr<TestSystem> createSystem(std::optional<size_t> threadRequirement = {}) {
      return makeSystem("test", [](SystemContext<TestEntity, ContextArgs...>&) {}, threadRequirement);
    };

    auto buildGraph(std::initializer_list<std::shared_ptr<TestSystem>> systems) {
      SystemList s(systems);
      return JobGraph::build(s);
    }

    void testSystemsExpectParallel(std::initializer_list<std::shared_ptr<TestSystem>> systems) {
      auto root = buildGraph(systems);

      Assert::IsTrue(root != nullptr);
      Assert::AreEqual(uint32_t(0), root->mTotalDependencies);
      Assert::AreEqual(systems.size(), root->mDependents.size());

      size_t i = 0;
      for(const auto& system : systems) {
        Assert::IsTrue(system.get() == root->mDependents[i]->mSystem.get());
        ++i;
      }
    }

    void testSystemsExpectSequential(std::initializer_list<std::shared_ptr<TestSystem>> systems) {
      auto root = buildGraph(systems);
      Assert::IsTrue(root != nullptr);
      Assert::AreEqual(size_t(1), root->mDependents.size(), L"Systems should have been sequential, meaning a single element chain", LINE_INFO());

      auto curNode = root;
      for(auto&& system : systems) {
        Assert::IsTrue(curNode != nullptr);
        auto it = std::find_if(curNode->mDependents.begin(), curNode->mDependents.end(), [&system](std::shared_ptr<JobInfo<TestEntity>>& info) {
          return system.get() == info->mSystem.get();
        });
        Assert::IsTrue(it != curNode->mDependents.end());

        curNode = *it;
      }
      Assert::IsTrue(curNode->mDependents.empty(), L"There should have been no trailing systems", LINE_INFO());
    }

    TEST_METHOD(JobGraph_NoSystems_EmptyRoot) {
      SystemList systems;
      auto job = JobGraph::build(systems);

      Assert::IsTrue(job != nullptr);
      Assert::AreEqual(job->mTotalDependencies, uint32_t(0));
      Assert::IsTrue(job->mDependents.empty());
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
      Assert::AreEqual(size_t(1), jNode->mDependents.size());
      Assert::IsTrue(k.get() == jNode->mDependents[0]->mSystem.get());

      //Validate k
      Assert::IsTrue(jNode->mDependents[0]->mDependents.empty());
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
      while(!jobs.empty()) {
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

      Assert::IsTrue(std::vector<TestSystem*>{ a.get(), b.get(), c.get(), d.get(), e.get(), f.get(), g.get() } == resultOrder);
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
  };
}