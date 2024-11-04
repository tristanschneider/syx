#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppBuilder.h"
#include "TestGame.h"
#include "SceneNavigator.h"
#include "RowTags.h"
#include "TableAdapters.h"
#include "glm/glm.hpp"
#include "TestSceneAsset.h"
#include "loader/AssetLoader.h"
#include "loader/AssetReader.h"
#include "loader/SceneAsset.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(AssetServiceTest) {
    struct Scene : SceneNavigator::IScene {
      void init(IAppBuilder&) final {
      }

      void update(IAppBuilder&) final {
      }
    };

    struct Game {
      void awaitCompletion(const Loader::AssetHandle& handle) {
        auto reader = Loader::createAssetReader(game.builder());
        auto start = std::chrono::steady_clock::now();
        //Time out after a second, should be enough to load anything reasonably
        while(std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
          game.update();
          std::this_thread::yield();
          switch(reader->getLoadState(handle).step) {
          case Loader::LoadStep::Succeeded:
            return;
          case Loader::LoadStep::Loading:
          case Loader::LoadStep::Requested:
            break;
          case Loader::LoadStep::Failed:
          case Loader::LoadStep::Invalid:
            Assert::Fail(L"Asset failed to load");
            break;
          }
        }
        Assert::Fail(L"Timed out waiting for asset");
      }

      TestGame game{ std::make_unique<Scene>() };
      AppTaskArgs args{ game.sharedArgs() };
    };

    static std::vector<uint8_t> toBytes(std::string_view view) {
      std::vector<uint8_t> result;
      result.resize(view.size());
      std::memcpy(result.data(), view.data(), view.size());
      return result;
    }

    /* TODO: Make it pass
    TEST_METHOD(LoadTestScene) {
      const std::string_view rawScene = TestAssets::getTestScene();
      Game game;
      auto task = game.game.builder();
      auto loader = Loader::createAssetLoader(task);
      Loader::AssetHandle handle = loader->requestLoad({ "test.glb" }, toBytes(rawScene));

      game.awaitCompletion(handle);

      CachedRow<Loader::SceneAssetRow> result;
      auto res = task.getResolver(result);
      auto ref = task.getIDResolver()->getRefResolver();
      auto resultRef = ref.tryUnpack(handle.asset);
      Assert::IsTrue(resultRef.has_value());
      Assert::IsTrue(res->tryGetOrSwapRow(result, *resultRef));
      const Loader::SceneAsset& scene = result->at(resultRef->getElementIndex());

      Assert::AreEqual(size_t(3), scene.materials.size());
      Assert::AreEqual(size_t(3), scene.meshes.size());
      Assert::AreEqual(size_t(1), scene.player.players.size());
      Assert::AreEqual(size_t(1), scene.terrain.terrains.size());
    }
    */
  };
}