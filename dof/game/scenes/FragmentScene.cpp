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
      std::random_device device;
      std::mt19937 generator(device());
      cameraModifier->resize(1);
      const size_t cameraIndex = 0;
      Camera& mainCamera = cameras.get<0>(0).at(cameraIndex);
      const size_t cameraStableId = cameras.get<1>(0).at(cameraIndex);
      mainCamera.zoom = 1.5f;

      playerModifier->resize(1);
      const StableIDRow& playerStableRow = players.get<const StableIDRow>(0);
      //TODO: this could be built into the modifier itself
      const size_t playerIndex = 0;
      Events::onNewElement(StableElementID::fromStableRow(playerIndex, playerStableRow), args);

      //Random angle in sort of radians
      const float playerStartAngle = float(generator() % 360)*6.282f/360.0f;
      const float playerStartDistance = 25.0f;
      //Start way off the screen, the world boundary will fling them into the scene
      players.get<0>(0).at(0) = playerStartDistance*std::cos(playerStartAngle);
      players.get<1>(0).at(0) = playerStartDistance*std::sin(playerStartAngle);

      //Make the camera follow the player
      auto follow = TableAdapters::getFollowTargetByPositionEffects(args);
      const size_t id = TableAdapters::addStatEffectsSharedLifetime(follow.base, StatEffect::INFINITE, &cameraStableId, 1);
      follow.command->at(id).mode = FollowTargetByPositionStatEffect::FollowMode::Interpolation;
      follow.base.target->at(id) = StableElementID::fromStableID(playerStableRow.at(playerIndex));
      follow.base.curveDefinition->at(id) = &Config::getCurve(config->camera.followCurve);

      //Load ability from config
      Player::initAbility(*config, std::get<2>(players.rows));
    });
    builder.submitTask(std::move(task));
  }


  void setupFragmentScene(IAppBuilder& builder, Fragment::SceneArgs args) {
    using namespace Tags;
    auto task = builder.createTask();
    task.setName("Fragment Setup");
    SceneState* scene = task.query<SharedRow<SceneState>>().tryGetSingletonElement();
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
    if(!scene) {
      task.discard();
      return;
    }

    task.setCallback([scene, fragments, modifiers, args, terrain, terrainModifier](AppTaskArgs& taskArgs) mutable {
      std::random_device device;
      std::mt19937 generator(device());

      //Add some arbitrary objects for testing
      const size_t rows = args.mFragmentRows;
      const size_t columns = args.mFragmentColumns;
      const size_t total = rows*columns;
      const float startX = -float(columns)/2.0f;
      const float startY = -float(rows)/2.0f;
      const float scaleX = 1.0f/float(columns);
      const float scaleY = 1.0f/float(rows);
      for(size_t t = 0; t < fragments.size(); ++t) {
        modifiers[t]->resize(total);
        auto tableRows = fragments.get(t);
        const StableIDRow& stableIDs = fragments.get<const StableIDRow>(t);
        for(size_t s = 0; s < stableIDs.size(); ++s) {
          Events::onNewElement(StableElementID::fromStableRow(s, stableIDs), taskArgs);
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

        for(size_t j = 0; j < total; ++j) {
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

      if(terrain.size()) {
        const size_t ground = terrainModifier[0]->addElements(1);
        auto [_, px, py, pz, sx, sy, stable] = terrain.get(0);
        Events::onNewElement(StableElementID::fromStableRow(ground, *stable), taskArgs);
        const glm::vec2 scale = scene->mBoundaryMax - scene->mBoundaryMin;
        const glm::vec2 center = (scene->mBoundaryMin + scene->mBoundaryMax) * 0.5f;
        TableAdapters::write(ground, center, *px, *py);
        TableAdapters::write(ground, scale, *sx, *sy);
        pz->at(ground) = -0.1f;
      }
    });
    builder.submitTask(std::move(task));
  }

  void setupFragmentScene(IAppBuilder& builder) {
    auto temp = builder.createTask();
    const Config::GameConfig* config = temp.query<const SharedRow<Config::GameConfig>>().tryGetSingletonElement();
    temp.discard();

    Fragment::SceneArgs args;
    args.mFragmentRows = config->fragment.fragmentRows;
    args.mFragmentColumns = config->fragment.fragmentColumns;
    setupFragmentScene(builder, args);
  }

  struct FragmentScene : SceneNavigator::IScene {
    void init(IAppBuilder& builder) final {
      setupPlayer(builder);
      setupFragmentScene(builder);
    }
    void update(IAppBuilder&) final {}
    void uninit(IAppBuilder&) final {}
  };

  std::unique_ptr<SceneNavigator::IScene> createFragmentScene() {
    return std::make_unique<FragmentScene>();
  }
}