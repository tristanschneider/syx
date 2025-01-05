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
      AppTaskArgs& args{ game.sharedArgs() };
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
        .sampleMode = Loader::TextureSampleMode::LinearInterpolation,
        .format = Loader::TextureFormat::RGBA,
        .buffer = std::vector<uint8_t>(50*50*4)
      }
    };

    inline static const Loader::MeshAsset PLAYER_MESH{
      .verts = std::vector<Loader::MeshVertex>{
        { glm::vec2{ -1, -1 }, glm::vec2{ 0, 0 } },
        { glm::vec2{ 1, -1 }, glm::vec2{ 1, 0 } },
        { glm::vec2{ 1, 1 }, glm::vec2{ 1, 1 } },

        { glm::vec2{ -1, -1 }, glm::vec2{ 0, 0 } },
        { glm::vec2{ 1, 1 }, glm::vec2{ 1, 1 } },
        { glm::vec2{ -1, 1 }, glm::vec2{ 0, 1 } }
      }
    };

    inline static const Loader::MaterialAsset GROUND_MATERIAL{
      .texture = Loader::TextureAsset {
        .width = 10,
        .height = 10,
        .sampleMode = Loader::TextureSampleMode::SnapToNearest,
        .format = Loader::TextureFormat::RGBA,
        .buffer = std::vector<uint8_t>(10*10*4)
      }
    };

    inline static const Loader::MeshAsset GROUND_MESH{
      .verts = std::vector<Loader::MeshVertex>{
        { glm::vec2{ -1, -1 }, glm::vec2{ -1.5, -1.5 } },
        { glm::vec2{ 1, -1 }, glm::vec2{ 2.5, -1.5 } },
        { glm::vec2{ 1, 1 }, glm::vec2{ 2.5, 2.5 } },

        { glm::vec2{ -1, -1 }, glm::vec2{ -1.5, -1.5 } },
        { glm::vec2{ 1, 1 }, glm::vec2{ 2.5, 2.5 } },
        { glm::vec2{ -1, 1 }, glm::vec2{ -1.5, 2.5 } },
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

    struct SceneAssets {
      SceneAssets(RuntimeDatabaseTaskBuilder& task, const Loader::SceneAsset& scene) {
        CachedRow<Loader::MaterialAssetRow> materialRow;
        CachedRow<Loader::MeshAssetRow> meshRow;
        auto resolver = task.getResolver(materialRow, meshRow);
        ElementRefResolver res = task.getIDResolver()->getRefResolver();

        materials.resize(scene.materials.size());
        std::transform(scene.materials.begin(), scene.materials.end(), materials.begin(), [&](const Loader::AssetHandle& handle) {
          return resolver->tryGetOrSwapRowElement(materialRow, res.tryUnpack(handle.asset));
        });

        meshes.resize(scene.meshes.size());
        std::transform(scene.meshes.begin(), scene.meshes.end(), meshes.begin(), [&](const Loader::AssetHandle& handle) {
          return resolver->tryGetOrSwapRowElement(meshRow, res.tryUnpack(handle.asset));
        });
      }

      std::vector<Loader::MaterialAsset*> materials;
      std::vector<Loader::MeshAsset*> meshes;
    };

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
      SceneAssets assets{ task, scene };

      Assert::AreEqual(size_t(3), scene.materials.size());
      //Hack to compare only container sizes, expected are all zeroes
      for(Loader::MaterialAsset* m : assets.materials) {
        if(m) {
          std::fill(m->texture.buffer.begin(), m->texture.buffer.end(), uint8_t{});
        }
      }
      Assert::IsTrue(PLAYER_MATERIAL == *assets.materials[0]);
      Assert::IsTrue(GROUND_MATERIAL == *assets.materials[1]);

      Assert::AreEqual(size_t(2), assets.meshes.size());
      Assert::IsTrue(assets.meshes[0] && PLAYER_MESH == *assets.meshes[0]);
      Assert::IsTrue(assets.meshes[1] && GROUND_MESH == *assets.meshes[1]);

      Assert::AreEqual(size_t(1), scene.player.players.size());
      Assert::IsTrue(Loader::MeshIndex{ 0, 0 } == scene.player.meshIndex);
      const Loader::Player& p = scene.player.players[0];
      assertEq(glm::vec3{ 14.6805, -12.0115, 9.118 }, p.transform.pos);
      Assert::AreEqual(-0.275905281f, p.transform.rot);
      Assert::AreEqual(uint8_t(0b01111011), p.collisionMask.mask);
      Assert::AreEqual(uint8_t(0b10001001), p.constraintMask.mask);
      assertEq(glm::vec3{ 0.371f, 0.f, 0.175f }, p.velocity.linear);
      Assert::AreEqual(0.1f, p.velocity.angular, 0.001f);
      Assert::AreEqual(0.1f, scene.player.thickness.thickness);

      Assert::AreEqual(size_t(1), scene.terrain.terrains.size());
      Assert::IsTrue(Loader::MeshIndex{ 1, 1 } == scene.terrain.meshIndex);
      const Loader::Terrain& t = scene.terrain.terrains[0];
      assertEq(glm::vec3{ 0.f, 0.f, -0.1f }, t.transform.pos);
      Assert::AreEqual(0.0f, t.transform.rot);
      assertEq(glm::vec2{ 28, 29 }, t.scale.scale);
      Assert::AreEqual(0.2f, scene.terrain.thickness.thickness);
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