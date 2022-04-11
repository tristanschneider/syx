#include "Precompile.h"

#include "ecs/component/MessageComponent.h"
#include "ecs/component/PlatformMessageComponents.h"
#include "ecs/system/AssetSystem.h"
#include "test/TestAppContext.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SystemTests {
  TEST_CLASS(AssetSystemTests) {
    struct TestAsset {
    };

    enum class LoadResult {
      AlwaysSucceed,
      AlwaysFail,
    };

    struct AssetTestAppContext : TestAppContext {
      AssetTestAppContext(LoadResult result) {
        auto sim = mContext.getUpdatePhase(Engine::AppPhase::Simulation);
        switch(result) {
          case LoadResult::AlwaysSucceed:
            sim.mSystems.push_back(AssetSystem::instantAssetLoadSystem<&testAssetLoader>(""));
            break;
          case LoadResult::AlwaysFail:
            sim.mSystems.push_back(AssetSystem::instantAssetLoadSystem<&testFailAssetLoader>(""));
            break;
        }
        mContext.registerUpdatePhase(Engine::AppPhase::Simulation, std::move(sim.mSystems), sim.mTargetFPS);
        mContext.buildExecutionGraph();
      }

      static std::optional<TestAsset> testAssetLoader(std::vector<uint8_t>&) {
        return TestAsset{};
      }

      static std::optional<TestAsset> testFailAssetLoader(std::vector<uint8_t>&) {
        return {};
      }
    };

    TEST_METHOD(TestAssetSystem_RequestAssetLoad_Loads) {
      AssetTestAppContext app(LoadResult::AlwaysSucceed);
      app.mFS->mFiles["test"] = "contents";
      auto&& [ entity, drop, msg ] = app.mRegistry.createAndGetEntityWithComponents<OnFilesDroppedMessageComponent, MessageComponent>();
      drop.get().mFiles.push_back(FilePath("test"));

      app.update();

      auto it = app.mRegistry.begin<AssetComponent>();
      Assert::IsTrue(it != app.mRegistry.end<AssetComponent>());
      Assert::IsTrue(app.mRegistry.hasComponent<TestAsset>(it.entity()));
    }

    TEST_METHOD(TestAssetSystem_DropFile_LoadsAsset) {
      AssetTestAppContext app(LoadResult::AlwaysSucceed);
      app.mFS->mFiles["test"] = "contents";
      auto&& [ entity, loadReq ] = app.mRegistry.createAndGetEntityWithComponents<AssetLoadRequestComponent>();
      loadReq.get().mPath = FilePath("test");

      app.update();

      Assert::IsTrue(app.mRegistry.isValid(entity));
      Assert::IsTrue(app.mRegistry.hasComponent<TestAsset>(entity));
    }

    TEST_METHOD(TestAssetSystemFail_RequestAssetLoad_EntityRemoved) {
      AssetTestAppContext app(LoadResult::AlwaysFail);
      app.mFS->mFiles["test"] = "contents";
      auto&& [ entity, loadReq ] = app.mRegistry.createAndGetEntityWithComponents<AssetLoadRequestComponent>();
      loadReq.get().mPath = FilePath("test");

      app.update();
      //Entity should be destroyed at the beginning of the second tick
      app.update();

      Assert::IsFalse(app.mRegistry.isValid(entity));
    }

    TEST_METHOD(TestAssetSystemNoFile_RequestAssetLoad_EntityRemoved) {
      AssetTestAppContext app(LoadResult::AlwaysSucceed);
      auto&& [ entity, loadReq ] = app.mRegistry.createAndGetEntityWithComponents<AssetLoadRequestComponent>();
      loadReq.get().mPath = FilePath("test");

      app.update();
      //Entity should be destroyed at the beginning of the second tick
      app.update();

      Assert::IsFalse(app.mRegistry.isValid(entity));
    }
  };
}