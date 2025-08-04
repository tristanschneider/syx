#include "Precompile.h"
#include "scenes/PerformanceScenes.h"

#include "RowTags.h"
#include "SceneNavigator.h"
#include "AppBuilder.h"
#include "ConstraintSolver.h"
#include "Events.h"
#include "TableAdapters.h"
#include "Physics.h"
#include <math/Geometric.h>

namespace Scenes {
  struct SingleStack : SceneNavigator::IScene {
    void init(IAppBuilder& builder) final {
      auto task = builder.createTask();
      task.setName("singlestack init");
      auto objs = task.query<
        Tags::DynamicPhysicsObjectsTag,
        Tags::PosXRow,
        Tags::PosYRow,
        Tags::ScaleXRow,
        Tags::ScaleYRow,
        AccelerationY,
        Events::EventsRow
      >();
      auto terrain = task.query<
        Tags::TerrainRow,
        Tags::PosXRow,
        Tags::PosYRow,
        Tags::RotXRow,
        Tags::RotYRow,
        Tags::ScaleXRow,
        Tags::ScaleYRow,
        Events::EventsRow
      >();
      if(!objs.size() || !terrain.size()) {
        task.discard();
        return;
      }
      auto objsModifier = task.getModifierForTable(objs[0]);
      auto terrainModifier = task.getModifierForTable(terrain[0]);
      auto ids = task.getIDResolver();

      task.setCallback([ids, objs, terrain, objsModifier, terrainModifier](AppTaskArgs&) mutable {
        constexpr size_t countX = 100;
        constexpr size_t countY = 100;
        const glm::vec2 size{ 1, 1 };
        const glm::vec2 origin{ 0, 0 };
        const float gravity = -0.005f;
        objsModifier->resize(countX*countY);
        {
          auto [tag, px, py, sx, sy, gy, stable] = objs.get(0);
          for(size_t x = 0; x < countX; ++x) {
            for(size_t y = 0; y < countY; ++y) {
              glm::vec2 pos = origin + size * glm::vec2{ static_cast<float>(x), static_cast<float>(y) };
              if(x > 3 && x < 5) {
                pos.y += 3.0f;
              }
              const size_t i = x*countX + y;
              stable->getOrAdd(i).setCreate();
              TableAdapters::write(i, pos, *px, *py);
              TableAdapters::write(i, size, *sx, *sy);
              gy->at(i) = gravity;
            }
          }
        }
        {
          auto [tag, px, py, rx, ry, sx, sy, stable] = terrain.get(0);
          const float groundHeight = 2.0f;
          const float widthBuffer = 3.0f;
          const glm::vec2 groundSize{ widthBuffer*2.0f + static_cast<float>(countX)*size.x, groundHeight };
          std::array positions = {
            origin + glm::vec2{ groundSize.x/2.0f - widthBuffer, -groundSize.y },
            origin + glm::vec2{ -static_cast<float>(countX)*size.x*0.1f, groundSize.x/2.0f - 3.0f },
            origin + glm::vec2{ groundSize.x + static_cast<float>(countX)*size.x*0.01f, groundSize.x/2.0f - 3.0f },
          };
          std::array rotations = {
            glm::vec2{ 1, 0 },
            Geo::directionFromAngle(Constants::PI2 + 0.1f),
            Geo::directionFromAngle(Constants::PI2 - 0.1f)
          };
          terrainModifier->resize(positions.size());
          for(size_t i = 0; i < positions.size(); ++i) {
            stable->getOrAdd(i).setCreate();
            TableAdapters::write(i, positions[i], *px, *py);
            TableAdapters::write(i, rotations[i], *rx, *ry);
            TableAdapters::write(i, groundSize, *sx, *sy);
          }
        }
      });

      builder.submitTask(std::move(task));
    }
    void update(IAppBuilder&) final {}
    void uninit(IAppBuilder&) final {}
  };

  std::unique_ptr<SceneNavigator::IScene> createSingleStack() {
    return std::make_unique<SingleStack>();
  }
}