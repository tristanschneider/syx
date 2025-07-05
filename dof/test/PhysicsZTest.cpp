#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppBuilder.h"
#include "Events.h"
#include "Narrowphase.h"
#include "Simulation.h"
#include "SceneNavigator.h"
#include "TableAdapters.h"
#include "TestGame.h"
#include "ConstraintSolver.h"
#include "IGame.h"

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
          const StableIDRow,
          Events::EventsRow
        >();
        auto terrainModifier = task.getModifiersForTables(terrain.getMatchingTableIDs());
        auto ids = task.getIDResolver();

        auto dynamics = task.query<
          const DynamicPhysicsObjectsWithZTag,
          FloatRow<Pos, X>,
          FloatRow<Pos, Y>,
          FloatRow<Pos, Z>,
          ScaleXRow,
          ScaleYRow,
          ConstraintSolver::MassRow,
          const StableIDRow,
          Events::EventsRow
        >();
        auto dynamicsModifier = task.getModifiersForTables(dynamics);

        task.setCallback([=](AppTaskArgs&) mutable {
          {
            const size_t ground = terrainModifier[0]->addElements(1);
            auto [_, px, py, pz, sx, sy, stable, events] = terrain.get(0);
            data.ground = stable->at(ground);
            events->getOrAdd(ground).setCreate();
            const glm::vec2 scale{ 2.0f };
            const glm::vec2 center{ 0.0f };
            TableAdapters::write(ground, center, *px, *py);
            TableAdapters::write(ground, scale, *sx, *sy);
            pz->at(ground) = 0.f;
          }
          {
            const size_t dy = dynamicsModifier[0]->addElements(1);
            auto [_, px, py, pz, sx, sy, mass, stable, events] = dynamics.get(0);
            data.dynamicObject = stable->at(dy);
            events->getOrAdd(dy).setCreate();
            const glm::vec2 scale{ 1.0f };
            const glm::vec2 center{ 0.0f };
            TableAdapters::write(dy, center, *px, *py);
            TableAdapters::write(dy, scale, *sx, *sy);
            mass->at(dy) = Mass::computeQuadMass(Mass::Quad{ .fullSize = scale }).body;
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
        , refs{ game.game->getDatabase().getRuntime().getDescription() }
      {
        resolver = game.builder().getResolver<Row<int>>();
        ids = game.builder().getIDResolver();

        //Navigate to the scene
        game.update();
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

    struct TestObject {
      TestObject(ITableResolver& resolver, const UnpackedDatabaseElementID& d) {
        CachedRow<Tags::PosZRow> posZ;
        CachedRow<Tags::PosXRow> posX;
        CachedRow<Tags::PosYRow> posY;
        CachedRow<Tags::LinVelZRow> linVelZ;
        CachedRow<Tags::LinVelXRow> linVelX;
        CachedRow<Tags::LinVelYRow> linVelY;
        CachedRow<Narrowphase::SharedThicknessRow> sharedThickness;
        dPosX = resolver.tryGetOrSwapRowElement(posX, d);
        dPosY = resolver.tryGetOrSwapRowElement(posY, d);
        dPos = resolver.tryGetOrSwapRowElement(posZ, d);
        dVel = resolver.tryGetOrSwapRowElement(linVelZ, d);
        dVelX = resolver.tryGetOrSwapRowElement(linVelX, d);
        dVelY = resolver.tryGetOrSwapRowElement(linVelY, d);
        dThickness = resolver.tryGetOrSwapRowElement(sharedThickness, d);
      }

      void reset() {
        *dPosX = *dPosY = *dPos = *dVel = *dVelX = *dVelY = 0.0f;
      }

      glm::vec2 getPos() const {
        return { *dPosX, *dPosY };
      }

      glm::vec2 getVelocity() const {
        return { *dVelX, *dVelY };
      }

      const TestObject& assertCollisionAtZWithVelocity(TestGame& game, float expectedPos, float expectedVelocity) const {
        Assert::AreEqual(expectedPos, *dPos, E, L"Should collide with ground");
        Assert::AreEqual(expectedVelocity, *dVel, E, L"Velocity should have been truncated to collision distance");
        game.update();
        Assert::AreEqual(expectedPos, *dPos, E);
        Assert::AreEqual(0.0f, *dVel, E);
        return *this;
      }

      const TestObject& assertZUnimpeded(float expectedPos, float expectedVel) const {
        Assert::AreEqual(expectedPos, *dPos, E, L"Should have been unchanged since no Z solving should take place");
        Assert::AreEqual(expectedVel, *dVel, E, L"Should be unchanged");
        return *this;
      }

      const TestObject& assertXYSolved() const {
        Assert::IsTrue(glm::length(getPos()) > 0.0f);
        Assert::IsTrue(glm::length(getVelocity()) > 0.0f);
        return *this;
      }

      float* dPosX{};
      float* dPosY{};
      float* dPos{};
      float* dVel{};
      float* dVelX{};
      float* dVelY{};
      float* dThickness{};
    };

    static constexpr auto POS_AND_NEG = { -1.0f, 1.0f };

    TEST_METHOD(ZeroToZeroThickness) {
      Game game;
      TestObject dy{ *game.resolver, game.refs.uncheckedUnpack(game.data.dynamicObject) };
      game.setAllThickness(0.0f);

      //Speed to go through the ground
      for(float dir : POS_AND_NEG) {
        *dy.dPos = 10.0f*dir;
        *dy.dVel = -20.0f*dir;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 0.0f, -10.0f*dir);
      }

      //Speed to exactly hit the ground
      for(float dir : POS_AND_NEG) {
        *dy.dPos = 10.0f*dir;
        *dy.dVel = -10.0f*dir;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 0.0f, -10.0f*dir);
      }

      //Exactly touching
      for(float dir : POS_AND_NEG) {
        dy.reset();
        *dy.dPos = 0.0f;
        *dy.dVel = 1.0f*dir;

        game.game.update();

        dy.assertZUnimpeded(1.0f*dir, 1.0f*dir).assertXYSolved();
      }
    }

    TEST_METHOD(ZeroToNonZeroThickness) {
      Game game;
      TestObject dy{ *game.resolver, game.refs.uncheckedUnpack(game.data.dynamicObject) };
      TestObject stat{ *game.resolver, game.refs.uncheckedUnpack(game.data.ground) };
      *dy.dThickness = 0.0f;
      *stat.dThickness = 1.0f;

      //Speed to go down through the ground
      {
        *dy.dPos = 10.0f;
        *dy.dVel = -20.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 1.0f, -9.0f);
      }
      //Speed to go up through the ground
      {
        *dy.dPos = -10.0f;
        *dy.dVel = 20.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 0.0f, 10.0f);
      }

      //Speed to exactly hit the ground downward
      {
        *dy.dPos = 10.0f;
        *dy.dVel = -9.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 1.0f, -9.0f);
      }
      //Speed to exactly hit the ground upward
      {
        *dy.dPos = -10.0f;
        *dy.dVel = 10.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 0.0f, 10.0f);
      }

      //Exactly touching edges and middle
      for(float pos : { 0.0f, 1.0f, 0.5f }) {
        for(float dir : POS_AND_NEG) {
          dy.reset();

          *dy.dPos = pos;
          *dy.dVel = dir;

          game.game.update();

          dy.assertZUnimpeded(pos + dir, dir).assertXYSolved();
        }
      }
    }

    TEST_METHOD(NonZeroThickness) {
      Game game;
      TestObject dy{ *game.resolver, game.refs.uncheckedUnpack(game.data.dynamicObject) };
      TestObject stat{ *game.resolver, game.refs.uncheckedUnpack(game.data.ground) };
      const float ta = 0.4f;
      const float tb = 0.5f;
      *dy.dThickness = ta;
      *stat.dThickness = tb;

      //Speed to go down through the ground
      {
        *dy.dPos = 10.0f;
        *dy.dVel = -20.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, tb, -10.0f + tb);
      }
      //Speed to go up through the ground
      {
        *dy.dPos = -10.0f;
        *dy.dVel = 20.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, -ta, 10.0f - ta);
      }

      //Speed to exactly hit the ground downward
      {
        *dy.dPos = 10.0f;
        *dy.dVel = -10.0f + tb;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, tb, -10.0f + tb);
      }
      //Speed to exactly hit the ground upward
      {
        *dy.dPos = -10.0f;
        *dy.dVel = 10.0f - ta;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, -ta, 10.0f - ta);
      }

      //Exactly touching edges and middle
      for(float pos : { 0.0f, tb, -ta }) {
        for(float dir : POS_AND_NEG) {
          dy.reset();

          *dy.dPos = pos;
          *dy.dVel = dir;

          game.game.update();

          dy.assertZUnimpeded(pos + dir, dir).assertXYSolved();
        }
      }
    }
  };
}