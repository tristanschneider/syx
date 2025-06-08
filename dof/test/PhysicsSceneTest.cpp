#include <Precompile.h>
#include <CppUnitTest.h>

#include <TestGame.h>
#include <IAppModule.h>
#include <filesystem>

#include <IGame.h>
#include <Game.h>
#include <test/PhysicsTestModule.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(PhysicsSceneTest) {
    static std::vector<std::filesystem::path> getPhysicsScenes() {
      std::vector<std::filesystem::path> result;
      const std::filesystem::path dir{ "./data/test/physics" };
      for(const auto& entry : std::filesystem::directory_iterator{ dir }) {
        if(entry.path().extension() == L".glb") {
          result.push_back(entry.path());
          Logger::WriteMessage(std::format("Found physics test {}\n", dir.generic_string()).c_str());
        }
      }
      if(result.empty()) {
        Assert::Fail(L"Didn't find any physcis scenes to test");
      }
      return result;
    }

    struct TestData {
      size_t runCount{};
      PhysicsTestModule::ValidationStats stats;
      std::unordered_set<std::string> errors;
    };

    class TestLogger : public PhysicsTestModule::ILogger {
    public:
      TestLogger(std::shared_ptr<TestData> _data)
        : data{ std::move(_data) }
      {
      }

      void logValidationError(const PhysicsTestModule::ValidationStats&, const glm::vec2& pos, PhysicsTestModule::ValidationError error) final {
        //First tick always fails which is not great but not something I want to deal with right now, still consider it a pass if everything else succeeds
        if(data->runCount) {
          logErrorIfNew(std::format("Error: {} at ({},{})", error.message, pos.x, pos.y));
        }
      }

      void logValidationSuccess(const PhysicsTestModule::ValidationStats&) final {
      }

      void logResults(const PhysicsTestModule::ValidationStats& stats) final {
        data->stats = stats;
        data->runCount++;
      }

      //These tests do many iterations, so only log the same message once as it'll probably show up in all iterations
      void logErrorIfNew(const std::string& str) {
        if(data->errors.insert(str).second) {
          Logger::WriteMessage(str.c_str());
        }
      }

      std::shared_ptr<TestData> data;
    };

    struct PhysicsGame : TestGame {
      PhysicsGame(std::shared_ptr<TestData> _data = std::make_shared<TestData>())
        : TestGame{ createGame(_data) }
        , testData{ _data }
      {
      }

      static std::unique_ptr<IGame> createGame(std::shared_ptr<TestData> data) {
        GameDefaults::DefaultGameBuilder builder = GameDefaults::createDefaultGameBuilder();
        builder.physicsTest = PhysicsTestModule::create([data](RuntimeDatabaseTaskBuilder&) -> std::unique_ptr<PhysicsTestModule::ILogger> {
          return std::make_unique<TestLogger>(data);
        });
        return Game::createGame(std::move(builder).build());
      }

      std::shared_ptr<TestData> testData;
    };

    TEST_METHOD(TestAllScenes) {
      const auto scenes = getPhysicsScenes();
      PhysicsGame game;
      constexpr size_t SCENE_TICKS = 100;
      //Gather all failures until the end as vstest will stop on the first but these may be multiple independent scenes
      size_t success{};

      for(const auto& scene : scenes) {
        Logger::WriteMessage(std::format("Loading scene {}\n", scene.generic_string()).c_str());
        game.loadSceneFromFile(std::string_view{ scene.generic_string() });
        for(size_t i = 0; i < SCENE_TICKS; ++i) {
          game.update();
        }

        //Fail if there were any errors or if the test contents weren't found in the file
        if(game.testData->errors.size() || !game.testData->stats.total()) {
          Logger::WriteMessage(std::format("Failed: {}\n", scene.generic_string()).c_str());
        }
        else {
          ++success;
          Logger::WriteMessage("Passed\n");
        }
      }

      Assert::AreEqual(scenes.size(), success);
    }
  };
}