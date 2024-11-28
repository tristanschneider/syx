#include "Precompile.h"
#include "SceneNavigator.h"

#include "AppBuilder.h"
#include "GameInput.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "stat/AllStatEffects.h"
#include <random>
#include "Player.h"
#include "Fragment.h"
#include "World.h"

namespace Scenes {
  void setupPlayer(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Player setup");
    auto cameras = task.query<Row<Camera>, const StableIDRow>();
    if(!cameras.size()) {
      task.discard();
      return;
    }
    std::shared_ptr<ITableModifier> cameraModifier = task.getModifierForTable(cameras.matchingTableIDs.front());
    auto players = task.query<
      FloatRow<Tags::Pos, Tags::X>,
      FloatRow<Tags::Pos, Tags::Y>,
      GameInput::PlayerInputRow,
      const StableIDRow>();
    Config::GameConfig* config = task.query<SharedRow<Config::GameConfig>>().tryGetSingletonElement();
    std::shared_ptr<ITableModifier> playerModifier = task.getModifierForTable(players.matchingTableIDs.front());
    const SceneState* scene = task.query<const SharedRow<SceneState>>().tryGetSingletonElement();

    task.setCallback([scene, cameras, cameraModifier, players, playerModifier, config](AppTaskArgs& args) mutable {
      if(!config->fragment.playerSpawn) {
        return;
      }
      auto [posX, posY, input, stableId] = players.get(0);
      std::random_device device;
      std::mt19937 generator(device());
      cameraModifier->resize(1);
      const size_t cameraIndex = 0;
      Camera& mainCamera = cameras.get<0>(0).at(cameraIndex);
      const ElementRef cameraStableId = cameras.get<1>(0).at(cameraIndex);
      mainCamera.zoom = 1.5f;

      playerModifier->resize(1);
      const StableIDRow& playerStableRow = players.get<const StableIDRow>(0);
      //TODO: this could be built into the modifier itself
      const size_t playerIndex = 0;
      Events::onNewElement(playerStableRow.at(playerIndex), args);

      TableAdapters::write(0, *config->fragment.playerSpawn, *posX, *posY);

      //Player.cpp will connect the camera to the player and initialize abilities
    });
    builder.submitTask(std::move(task));
  }

  void setupFragmentScene(IAppBuilder& builder) {
    using namespace Tags;
    auto task = builder.createTask();
    task.setName("Fragment Setup");
    SceneState* scene = task.query<SharedRow<SceneState>>().tryGetSingletonElement();
    const Config::FragmentConfig* args = &task.query<const SharedRow<Config::GameConfig>>().tryGetSingletonElement()->fragment;
    auto fragments = task.query<
      FloatRow<Pos, X>,
      FloatRow<Pos, Y>,
      FloatRow<FragmentGoal, X>,
      FloatRow<FragmentGoal, Y>,
      Row<CubeSprite>,
      const StableIDRow
    >();
    auto terrain = task.query<
      const TerrainRow,
      FloatRow<Pos, X>,
      FloatRow<Pos, Y>,
      FloatRow<Pos, Z>,
      ScaleXRow,
      ScaleYRow,
      const StableIDRow
    >();
    auto modifiers = task.getModifiersForTables(fragments.matchingTableIDs);
    auto terrainModifier = task.getModifiersForTables(terrain.matchingTableIDs);
    auto foundTable = builder.queryTables<FragmentGoalFoundTableTag>();
    if(!scene) {
      task.discard();
      return;
    }

    task.setCallback([scene, fragments, modifiers, args, terrain, terrainModifier, foundTable](AppTaskArgs& taskArgs) mutable {
      std::random_device device;
      std::mt19937 generator(device());

      //Add some arbitrary objects for testing
      const size_t rows = args->fragmentRows;
      const size_t columns = args->fragmentColumns;
      const size_t total = rows*columns;
      const size_t totalCompleted = std::min(args->completedFragments, total);
      const float startX = -float(columns)/2.0f;
      const float startY = -float(rows)/2.0f;
      const float scaleX = 1.0f/float(columns);
      const float scaleY = 1.0f/float(rows);
      for(size_t t = 0; t < fragments.size(); ++t) {
        if(!total) {
          continue;
        }
        modifiers[t]->resize(total);
        auto tableRows = fragments.get(t);
        const StableIDRow& stableIDs = fragments.get<const StableIDRow>(t);
        for(size_t s = 0; s < stableIDs.size(); ++s) {
          Events::onNewElement(stableIDs.at(s), taskArgs);
        }

        auto& posX = *std::get<FloatRow<Pos, X>*>(tableRows);
        auto& posY = *std::get<FloatRow<Pos, Y>*>(tableRows);
        auto& goalX = *std::get<FloatRow<FragmentGoal, X>*>(tableRows);
        auto& goalY = *std::get<FloatRow<FragmentGoal, Y>*>(tableRows);
        //Shuffle indices randomly
        std::vector<size_t> indices(rows*columns);
        int counter = 0;
        std::generate(indices.begin(), indices.end(), [&counter] { return counter++; });
        std::shuffle(indices.begin(), indices.end(), generator);
        //Make sure none start at their completed index
        for(size_t j = 0; j < total; ++j) {
          if(indices[j] == j) {
            std::swap(indices[j], indices[(j + 1) % total]);
          }
        }

        for(size_t j = 0; j < total; ++j) {
          //Immediately complete the desired amount of fragments
          if(j < totalCompleted && foundTable.size()) {
            Events::onMovedElement(stableIDs.at(j), foundTable[0], taskArgs);
          }

          const size_t shuffleIndex = indices[j];
          CubeSprite& sprite = std::get<Row<CubeSprite>*>(tableRows)->at(j);
          const size_t row = j / columns;
          const size_t column = j % columns;
          const size_t shuffleRow = shuffleIndex / columns;
          const size_t shuffleColumn = shuffleIndex % columns;
          //Goal position and uv is based on original index, starting position is based on shuffled index
          sprite.uMin = float(column)/float(columns);
          sprite.vMin = float(row)/float(rows);
          sprite.uMax = sprite.uMin + scaleX;
          sprite.vMax = sprite.vMin + scaleY;

          goalX.at(j) = startX + sprite.uMin*float(columns);
          goalY.at(j) = startY + sprite.vMin*float(rows);

          posX.at(j) = startX + shuffleColumn;
          posY.at(j) = startY + shuffleRow;
        }

        const float boundaryPadding = 10.0f;
        const size_t first = 0;
        const size_t last = total - 1;
        scene->mBoundaryMin = glm::vec2(goalX.at(first), goalY.at(first)) - glm::vec2(boundaryPadding);
        scene->mBoundaryMax = glm::vec2(goalX.at(last), goalY.at(last)) + glm::vec2(boundaryPadding);
      }

      if(terrain.size() && args->addGround) {
        const size_t ground = terrainModifier[0]->addElements(1);
        auto [_, px, py, pz, sx, sy, stable] = terrain.get(0);
        Events::onNewElement(stable->at(ground), taskArgs);
        const glm::vec2 scale = scene->mBoundaryMax - scene->mBoundaryMin;
        const glm::vec2 center = (scene->mBoundaryMin + scene->mBoundaryMax) * 0.5f;
        TableAdapters::write(ground, center, *px, *py);
        TableAdapters::write(ground, scale, *sx, *sy);
        pz->at(ground) = -0.1f;
      }
    });
    builder.submitTask(std::move(task));
  }

  struct RespawnSettings {
    float outOfBoundsZ = -5.0f;
    float respawnZ = 10.0f;
    float burstRadius = 10.0f;
  };

  void outOfWorldRespawn(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("outOfWorldRespawn");
    auto query = task.query<
      const FloatRow<Tags::GPos, Tags::Z>,
      const StableIDRow
    >();
    RespawnSettings settings;

    task.setCallback([query, settings](AppTaskArgs& args) mutable {
      VelocityStatEffect::Builder velocity{ args };
      PositionStatEffect::Builder position{ args };
      FragmentBurstStatEffect::Builder burst{ args };
      for(size_t t = 0; t < query.size(); ++t) {
        auto [zs, stable] = query.get(t);
        for(size_t i = 0; i < zs->size(); ++i) {
          if(zs->at(i) < settings.outOfBoundsZ) {
            const auto* stableID = &stable->at(i);
            position.createStatEffects(1).setLifetime(StatEffect::INSTANT).setOwner(*stableID);
            position.setZ(settings.respawnZ);
            velocity.createStatEffects(1).setLifetime(StatEffect::INSTANT).setOwner(*stableID);
            velocity.setZ({ 0.0f });
            burst.createStatEffects(1).setLifetime(StatEffect::INFINITE).setOwner(*stableID);
            burst.setRadius(settings.burstRadius);
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  struct FragmentScene : SceneNavigator::IScene {
    void init(IAppBuilder& builder) final {
      setupPlayer(builder);
      setupFragmentScene(builder);
    }
    void update(IAppBuilder& builder) final {
      World::enforceWorldBoundary(builder);
      outOfWorldRespawn(builder);
    }
    void uninit(IAppBuilder&) final {}
  };

  std::unique_ptr<SceneNavigator::IScene> createFragmentScene() {
    return std::make_unique<FragmentScene>();
  }
}