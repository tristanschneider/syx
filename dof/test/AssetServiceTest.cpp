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
#include "TestAssert.h"
#include "ConstraintSolver.h"
#include "Narrowphase.h"

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

    static RuntimeTable& getOrAssertTable(RuntimeDatabase& db, std::string_view name) {
      const size_t hash = gnx::Hash::constHash(name);
      for(size_t i = 0; i < db.size(); ++i) {
        if(db[i].getType().value == hash) {
          return db[i];
        }
      }
      Assert::Fail(L"Should have found table");
    }

    template<IsRow Src>
    Src& getOrAssertRow(RuntimeTable& table, std::string_view name) {
      const DBTypeID hash = Loader::getDynamicRowKey<Src>(name);
      auto result = table.tryGet(hash);
      Assert::IsNotNull(result);
      return static_cast<Src&>(*result);
    }

    template<IsRow Src, Loader::IsLoadableRow Dst>
    Src& getOrAssertRow(RuntimeTable& table) {
      return getOrAssertRow<Src>(table, Dst::KEY);
    }

    template<Loader::IsLoadableRow Dst>
    Dst& getOrAssertRow(RuntimeTable& table) {
      return getOrAssertRow<Dst, Dst>(table);
    }

    static void assertMaterialMatch(const Loader::MaterialAsset* actual, const Loader::MaterialAsset& expected) {
      Assert::IsNotNull(actual);
      Loader::MaterialAsset copy{ *actual };
      //Hack to compare only container sizes, expected are all zeroes
      std::fill(copy.texture.buffer.begin(), copy.texture.buffer.end(), uint8_t{});
      Assert::IsTrue(copy == expected);
    }

    static void assertMeshMatch(const Loader::MeshAsset* actual, const Loader::MeshAsset& expected) {
      Assert::IsNotNull(actual);
      Assert::IsTrue(*actual == expected);
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
      RuntimeDatabase& sdb = *scene.db;

      CachedRow<Loader::MeshAssetRow> meshes;
      CachedRow<Loader::MaterialAssetRow> materials;

      {
        RuntimeTable& players = getOrAssertTable(sdb, "Player");
        Assert::AreEqual(size_t(1), players.size());
        const Loader::MatMeshRef& matMesh = getOrAssertRow<Loader::MatMeshRefRow>(players).at(0);
        assertMeshMatch(res->tryGetOrSwapRowElement(meshes, ref.unpack(matMesh.mesh.asset)), PLAYER_MESH);
        assertMaterialMatch(res->tryGetOrSwapRowElement(materials, ref.unpack(matMesh.material.asset)), PLAYER_MATERIAL);
        const Loader::Transform& t = getOrAssertRow<Loader::TransformRow>(players).at(0);
        assertEq(glm::vec3{ 14.6805, -12.0115, 9.118 }, t.pos);
        Assert::AreEqual(-0.275905281f, t.rot);
        const Loader::Bitfield collisionMask = getOrAssertRow<Loader::BitfieldRow, Narrowphase::CollisionMaskRow>(players).at(0);
        Assert::AreEqual(uint8_t(0b11011110), static_cast<uint8_t>(collisionMask));
        const Loader::Bitfield constraintMask = getOrAssertRow<Loader::BitfieldRow, ConstraintSolver::ConstraintMaskRow>(players).at(0);
        Assert::AreEqual(uint8_t(0b10010001), static_cast<uint8_t>(constraintMask));
        const glm::vec4 v = getOrAssertRow<Loader::Vec4Row>(players, "Velocity3D").at(0);
        assertEq(glm::vec4{ 0.371f, 0.f, 0.175f, 0.1f }, v);
        assertEq(glm::vec3{ 1.f, 1.f, 1.0f }, t.scale);
        const float sharedThickness = getOrAssertRow<Loader::SharedFloatRow, Narrowphase::SharedThicknessRow>(players).at();
        Assert::AreEqual(0.1f, sharedThickness, 0.001f);
      }

      {
        RuntimeTable& terrains = getOrAssertTable(sdb, "Terrain");
        Assert::AreEqual(size_t(1), terrains.size());

        const Loader::MatMeshRef& matMesh = getOrAssertRow<Loader::MatMeshRefRow>(terrains).at(0);
        assertMeshMatch(res->tryGetOrSwapRowElement(meshes, ref.unpack(matMesh.mesh.asset)), GROUND_MESH);
        assertMaterialMatch(res->tryGetOrSwapRowElement(materials, ref.unpack(matMesh.material.asset)), GROUND_MATERIAL);

        const Loader::Transform& t = getOrAssertRow<Loader::TransformRow>(terrains).at(0);
        assertEq(glm::vec3{ 0.f, 0.f, -0.1f }, t.pos);
        Assert::AreEqual(0.0f, t.rot);
        assertEq(glm::vec3{ 28.f, 29.f, 1.f }, t.scale);
        const float sharedThickness = getOrAssertRow<Loader::SharedFloatRow, Narrowphase::SharedThicknessRow>(terrains).at();
        Assert::AreEqual(0.2f, sharedThickness, 0.001f);
      }
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

    TEST_METHOD(MultipleConcurrentRequests) {
      const std::string_view rawScene = TestAssets::getTestScene();
      Game game;
      auto task = game.game.builder();
      auto loader = Loader::createAssetLoader(task);
      std::vector<Loader::AssetHandle> handles;
      for(size_t i = 0; i < 3; ++i) {
        handles.push_back(loader->requestLoad({ "test.glb" }, toBytes(rawScene)));
      }
      auto reader = Loader::createAssetReader(task);

      for(const auto& handle : handles) {
        game.awaitCompletion(handle, Loader::LoadStep::Succeeded);

        CachedRow<Loader::SceneAssetRow> result;
        auto res = task.getResolver(result);
        auto ref = task.getIDResolver()->getRefResolver();
        auto resultRef = ref.tryUnpack(handle.asset);
        Assert::IsTrue(resultRef.has_value());
        Assert::IsTrue(res->tryGetOrSwapRow(result, *resultRef));
      }
    }
  };
}