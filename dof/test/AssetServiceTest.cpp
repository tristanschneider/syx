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
      static bool isCompleted(Loader::LoadStep step) {
        switch(step) {
          case Loader::LoadStep::Loading:
          case Loader::LoadStep::Requested:
            return false;
          case Loader::LoadStep::Succeeded:
          case Loader::LoadStep::Failed:
          case Loader::LoadStep::Invalid:
            return true;
        }
        return true;
      }

      void awaitCompletion(const Loader::AssetHandle& handle, Loader::LoadStep expectedStep) {
        auto reader = Loader::createAssetReader(game.builder());
        auto start = std::chrono::steady_clock::now();
        //Time out after a second, should be enough to load anything reasonably
        while(std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
          game.update();
          std::this_thread::yield();

          if(const Loader::LoadStep actualStep = reader->getLoadState(handle).step; isCompleted(actualStep)) {
            Assert::IsTrue(expectedStep == actualStep);
            return;
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

    inline static const Loader::MaterialAsset PLAYER_MATERIAL{
      .texture = Loader::TextureAsset {
        .width = 50,
        .height = 50,
        .sampleMode = Loader::TextureSampleMode::SnapToNearest,
        .format = Loader::TextureFormat::RGB,
        .buffer = std::vector<uint8_t>(50*50*3)
      }
    };

    inline static const Loader::MeshAsset PLAYER_MESH{
      .materialIndex = 0,
      .vertices = std::vector<glm::vec2>{
        glm::vec2{ -1, 1 },
        glm::vec2{ 1, 1 },
        glm::vec2{ 1, -1 },
        glm::vec2{ -1, -1 }
      },
      .textureCoordinates = std::vector<glm::vec2>{
        glm::vec2{ 0, 0 },
        glm::vec2{ 1, 0 },
        glm::vec2{ 1, 1 },
        glm::vec2{ 0, 1 }
      }
    };

    inline static const Loader::MaterialAsset GROUND_MATERIAL{
      .texture = Loader::TextureAsset {
        .width = 10,
        .height = 10,
        .sampleMode = Loader::TextureSampleMode::SnapToNearest,
        .format = Loader::TextureFormat::RGB,
        .buffer = std::vector<uint8_t>(10*10*3)
      }
    };

    inline static const Loader::MeshAsset GROUND_MESH{
      .materialIndex = 1,
      .vertices = std::vector<glm::vec2>{
        glm::vec2{ -1, 1 },
        glm::vec2{ 1, 1 },
        glm::vec2{ 1, -1 },
        glm::vec2{ -1, -1 }
      },
      .textureCoordinates = std::vector<glm::vec2>{
        glm::vec2{ -1.5, -1.5 },
        glm::vec2{ 2.5, -1.5 },
        glm::vec2{ 2.5, 2.5 },
        glm::vec2{ -1.5, 2.5 },
      }
    };

    static void assertEq(const glm::vec3& a, const glm::vec3& b, float e = 0.0001f) {
      Assert::AreEqual(a.x, b.x, e);
      Assert::AreEqual(a.y, b.y, e);
      Assert::AreEqual(a.z, b.z, e);
    }

    static void assertEq(const glm::vec2& a, const glm::vec2& b, float e = 0.0001f) {
      Assert::AreEqual(a.x, b.x, e);
      Assert::AreEqual(a.y, b.y, e);
    }

    TEST_METHOD(LoadTestScene) {
      const std::string_view rawScene = TestAssets::getTestScene();
      Game game;
      auto task = game.game.builder();
      auto loader = Loader::createAssetLoader(task);
      Loader::AssetHandle handle = loader->requestLoad({ "test.glb" }, toBytes(rawScene));

      game.awaitCompletion(handle, Loader::LoadStep::Succeeded);

      CachedRow<Loader::SceneAssetRow> result;
      auto res = task.getResolver(result);
      auto ref = task.getIDResolver()->getRefResolver();
      auto resultRef = ref.tryUnpack(handle.asset);
      Assert::IsTrue(resultRef.has_value());
      Assert::IsTrue(res->tryGetOrSwapRow(result, *resultRef));
      Loader::SceneAsset& scene = result->at(resultRef->getElementIndex());

      Assert::AreEqual(size_t(3), scene.materials.size());
      //Hack to compare only container sizes, expected are all zeroes
      for(Loader::MaterialAsset& m : scene.materials) {
        std::fill(m.texture.buffer.begin(), m.texture.buffer.end(), uint8_t{});
      }
      Assert::IsTrue(PLAYER_MATERIAL == scene.materials[0]);
      Assert::IsTrue(GROUND_MATERIAL == scene.materials[1]);

      Assert::AreEqual(size_t(2), scene.meshes.size());
      Assert::IsTrue(PLAYER_MESH == scene.meshes[0]);
      Assert::IsTrue(GROUND_MESH == scene.meshes[1]);

      Assert::AreEqual(size_t(1), scene.player.players.size());
      Assert::IsTrue(Loader::MeshIndex{ 0 } == scene.player.meshIndex);
      const Loader::Player& p = scene.player.players[0];
      assertEq(glm::vec3{ 14.6805, 9.118, 12.0115 }, p.transform.pos);
      Assert::AreEqual(-0.275905281f, p.transform.rot);

      Assert::AreEqual(size_t(1), scene.terrain.terrains.size());
      Assert::IsTrue(Loader::MeshIndex{ 1 } == scene.terrain.meshIndex);
      const Loader::Terrain& t = scene.terrain.terrains[0];
      //TODO: should this be -0.1?
      assertEq(glm::vec3{ 0.f, 0.f, 0.1f }, t.transform.pos);
      Assert::AreEqual(0.0f, t.transform.rot);
      assertEq(glm::vec2{ 28, 1 }, t.scale.scale);
    }

    TEST_METHOD(LoadUnsupportedFileType) {
      const std::string_view rawScene = TestAssets::getTestScene();
      Game game;
      auto task = game.game.builder();
      auto loader = Loader::createAssetLoader(task);
      Loader::AssetHandle handle = loader->requestLoad({ "test.txt" }, toBytes("file contents"));
      auto reader = Loader::createAssetReader(task);

      game.awaitCompletion(handle, Loader::LoadStep::Failed);

      Assert::IsTrue(Loader::LoadStep::Failed == reader->getLoadState(handle).step);
    }

    TEST_METHOD(LoadInvalidFile) {
      const std::string_view rawScene = TestAssets::getTestScene();
      Game game;
      auto task = game.game.builder();
      auto loader = Loader::createAssetLoader(task);
      Loader::AssetHandle handle = loader->requestLoad({ "test.glb" }, toBytes("file contents"));
      auto reader = Loader::createAssetReader(task);

      game.awaitCompletion(handle, Loader::LoadStep::Failed);

      Assert::IsTrue(Loader::LoadStep::Failed == reader->getLoadState(handle).step);
    }
  };
}