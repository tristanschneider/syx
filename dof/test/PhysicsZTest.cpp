#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppBuilder.h"
#include "Narrowphase.h"
#include "Simulation.h"
#include "SceneNavigator.h"
#include "TableAdapters.h"
#include "TestGame.h"
#include "ConstraintSolver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(PhysicsZTest) {
    //Overlap tolerance from narrowphase times 2 with 0.00001 epsilon
    static constexpr float E = 0.02001f;
    struct SceneData {
      ElementRef ground;
      ElementRef dynamicObject;
    };
    struct TestScene : SceneNavigator::IScene {
      TestScene(SceneData& d)
        : data{ d } {
      }

      void init(IAppBuilder& builder) {
        using namespace Tags;
        auto task = builder.createTask();
        task.setName("test");

        auto terrain = task.query<
          const TerrainRow,
          FloatRow<Pos, X>,
          FloatRow<Pos, Y>,
          FloatRow<Pos, Z>,
          ScaleXRow,
          ScaleYRow,
          const StableIDRow
        >();
        auto terrainModifier = task.getModifiersForTables(terrain.matchingTableIDs);
        auto ids = task.getIDResolver();

        auto dynamics = task.query<
          const DynamicPhysicsObjectsWithZTag,
          FloatRow<Pos, X>,
          FloatRow<Pos, Y>,
          FloatRow<Pos, Z>,
          ScaleXRow,
          ScaleYRow,
          ConstraintSolver::MassRow,
          const StableIDRow
        >();
        auto dynamicsModifier = task.getModifiersForTables(dynamics.matchingTableIDs);

        task.setCallback([=](AppTaskArgs& taskArgs) mutable {
          {
            const size_t ground = terrainModifier[0]->addElements(1);
            auto [_, px, py, pz, sx, sy, stable] = terrain.get(0);
            data.ground = ids->tryResolveRef(StableElementID::fromStableRow(ground, *stable));
            Events::onNewElement(StableElementID::fromStableRow(ground, *stable), taskArgs);
            const glm::vec2 scale{ 2.0f };
            const glm::vec2 center{ 0.0f };
            TableAdapters::write(ground, center, *px, *py);
            TableAdapters::write(ground, scale, *sx, *sy);
            pz->at(ground) = 0.f;
          }
          {
            const size_t dy = dynamicsModifier[0]->addElements(1);
            auto [_, px, py, pz, sx, sy, mass, stable] = dynamics.get(0);
            data.dynamicObject = ids->tryResolveRef(StableElementID::fromStableRow(dy, *stable));
            Events::onNewElement(StableElementID::fromStableRow(dy, *stable), taskArgs);
            const glm::vec2 scale{ 1.0f };
            const glm::vec2 center{ 0.0f };
            TableAdapters::write(dy, center, *px, *py);
            TableAdapters::write(dy, scale, *sx, *sy);
            mass->at(dy) = Geo::computeQuadMass(scale.x, scale.y, 1.0f);
            pz->at(dy) = 10.0f;
          }
        });

        builder.submitTask(std::move(task));
      }

      SceneData& data;
    };

    struct Game {
      Game()
        : game{ std::make_unique<TestScene>(data) }
        , refs{ game.db->getRuntime().getDescription() }
      {
        resolver = game.builder().getResolver<Row<int>>();
        ids = game.builder().getIDResolver();

        //Navigate to the scene
        game.update();
      }

      void setAllThickness(float value) {
        game.builder().query<Narrowphase::SharedThicknessRow>().forEachElement([=](float& v) { v = value; });
      }

      SceneData data;
      TestGame game;
      std::shared_ptr<ITableResolver> resolver;
      std::shared_ptr<IIDResolver> ids;
      ElementRefResolver refs;
    };

    TEST_METHOD(DynamicAboveGround_VelocityTowardsGround_IsStoppedAtGround) {
      Game game;

      auto t = game.game.builder();
      auto d = game.refs.uncheckedUnpack(game.data.dynamicObject);
      CachedRow<Tags::PosZRow> posZ;
      CachedRow<Tags::LinVelZRow> linVelZ;
      float* dPos = game.resolver->tryGetOrSwapRowElement(posZ, d);
      float* dVel = game.resolver->tryGetOrSwapRowElement(linVelZ, d);
      game.setAllThickness(0.0f);

      //Speed to go through the ground
      *dPos = 10.0f;
      *dVel = -20.0f;

      game.game.update();

      Assert::AreEqual(0.0f, *dPos, E, L"Should collide with ground");
      Assert::AreEqual(-10.0f, *dVel, E, L"Velocity should have been truncated to collision distance");
      game.game.update();
      Assert::AreEqual(0.0f, *dPos, E);
      Assert::AreEqual(0.0f, *dVel, E);


      //Speed to exactly hit the ground
      *dPos = 10.0f;
      *dVel = -10.0f;

      game.game.update();

      Assert::AreEqual(0.0f, *dPos, E, L"Should collide with ground");
      Assert::AreEqual(-10.0f, *dVel, E, L"Velocity should have been truncated to collision distance");
      game.game.update();
      Assert::AreEqual(0.0f, *dPos, E);
      Assert::AreEqual(0.0f, *dVel, E);
    }
  };
}