#include "Precompile.h"
#include "scenes/PerformanceScenes.h"

#include "RowTags.h"
#include "SceneNavigator.h"
#include "AppBuilder.h"
#include "ConstraintSolver.h"
#include "DBEvents.h"
#include "TableAdapters.h"
#include "Physics.h"

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
        ConstraintSolver::MassRow,
        const StableIDRow
      >();
      auto terrain = task.query<
        Tags::TerrainRow,
        Tags::PosXRow,
        Tags::PosYRow,
        Tags::ScaleXRow,
        Tags::ScaleYRow,
        ConstraintSolver::SharedMassRow,
        const StableIDRow
      >();
      auto objsModifier = task.getModifierForTable(objs.matchingTableIDs[0]);
      auto terrainModifier = task.getModifierForTable(terrain.matchingTableIDs[0]);

      task.setCallback([objs, terrain, objsModifier, terrainModifier](AppTaskArgs& args) mutable {
        constexpr size_t countX = 100;
        constexpr size_t countY = 100;
        const glm::vec2 size{ 1, 1 };
        const glm::vec2 origin{ 0, 0 };
        const Geo::BodyMass quadMass = Geo::computeQuadMass(size.x, size.y, 1.0f);
        const Geo::BodyMass infMass = Geo::computeQuadMass(size.x, size.y, 0.0f);
        const float gravity = -0.005f;
        objsModifier->resize(countX*countY);
        Events::CreatePublisher create{ &args };
        {
          auto [tag, px, py, sx, sy, gy, mass, stable] = objs.get(0);
          for(size_t x = 0; x < countX; ++x) {
            for(size_t y = 0; y < countY; ++y) {
              glm::vec2 pos = origin + size * glm::vec2{ static_cast<float>(x), static_cast<float>(y) };
              if(x > 3 && x < 5) {
                pos.y += 3.0f;
              }
              const size_t i = x*countX + y;
              create(StableElementID::fromStableRow(i, *stable));
              TableAdapters::write(i, pos, *px, *py);
              TableAdapters::write(i, size, *sx, *sy);
              mass->at(i) = quadMass;
              gy->at(i) = gravity;
            }
          }
        }
        {
          auto [tag, px, py, sx, sy, mass, stable] = terrain.get(0);
          terrainModifier->resize(1);
          const size_t i = 0;
          create(StableElementID::fromStableRow(i, *stable));
          const float groundHeight = 2.0f;
          const float widthBuffer = 3.0f;
          const glm::vec2 groundSize{ widthBuffer*2.0f + static_cast<float>(countX)*size.x, groundHeight };
          TableAdapters::write(i, origin + glm::vec2{ groundSize.x/2.0f - widthBuffer, -groundSize.y/2.0f }, *px, *py);
          TableAdapters::write(i, groundSize, *sx, *sy);
          mass->at(i) = infMass;
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