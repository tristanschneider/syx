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
#include <transform/TransformRows.h>

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
          Transform::WorldTransformRow,
          const StableIDRow,
          Events::EventsRow
        >();
        auto terrainModifier = task.getModifiersForTables(terrain.getMatchingTableIDs());
        auto ids = task.getIDResolver();

        auto dynamics = task.query<
          const DynamicPhysicsObjectsWithZTag,
          Transform::WorldTransformRow,
          const StableIDRow,
          Events::EventsRow
        >();
        auto dynamicsModifier = task.getModifiersForTables(dynamics);

        task.setCallback([=](AppTaskArgs&) mutable {
          {
            const size_t ground = terrainModifier[0]->addElements(1);
            auto [_, transforms, stable, events] = terrain.get(0);
            data.ground = stable->at(ground);
            events->getOrAdd(ground).setCreate();
            const glm::vec2 scale{ 2.0f };
            const glm::vec2 center{ 0.0f };
            Transform::Parts parts;
            parts.translate = Geo::toVec3(center);
            parts.scale = scale;
            transforms->at(ground) = Transform::PackedTransform::build(parts);
          }
          {
            const size_t dy = dynamicsModifier[0]->addElements(1);
            auto [_, transforms, stable, events] = dynamics.get(0);
            data.dynamicObject = stable->at(dy);
            events->getOrAdd(dy).setCreate();
            const glm::vec2 scale{ 1.0f };
            const glm::vec2 center{ 0.0f };
            transforms->at(dy) = Transform::PackedTransform::build(Transform::Parts{
              .scale = scale,
              .translate = glm::vec3{ center.x, center.y, 10.f }
            });
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
        CachedRow<Transform::WorldTransformRow> transform;
        CachedRow<Tags::LinVelZRow> linVelZ;
        CachedRow<Tags::LinVelXRow> linVelX;
        CachedRow<Tags::LinVelYRow> linVelY;
        CachedRow<Narrowphase::SharedThicknessRow> sharedThickness;
        dTransform = resolver.tryGetOrSwapRowElement(transform, d);
        dVel = resolver.tryGetOrSwapRowElement(linVelZ, d);
        dVelX = resolver.tryGetOrSwapRowElement(linVelX, d);
        dVelY = resolver.tryGetOrSwapRowElement(linVelY, d);
        dThickness = resolver.tryGetOrSwapRowElement(sharedThickness, d);
      }

      void reset() {
        dTransform->setPos(glm::vec3{});
        *dVel = *dVelX = *dVelY = 0.0f;
      }

      glm::vec2 getPos() const {
        return dTransform->pos2();
      }

      glm::vec2 getVelocity() const {
        return { *dVelX, *dVelY };
      }

      const TestObject& assertCollisionAtZWithVelocity(TestGame& game, float expectedPos, float expectedVelocity) const {
        Assert::AreEqual(expectedPos, dTransform->tz, E, L"Should collide with ground");
        Assert::AreEqual(expectedVelocity, *dVel, E, L"Velocity should have been truncated to collision distance");
        game.update();
        Assert::AreEqual(expectedPos, dTransform->tz, E);
        Assert::AreEqual(0.0f, *dVel, E);
        return *this;
      }

      const TestObject& assertZUnimpeded(float expectedPos, float expectedVel) const {
        Assert::AreEqual(expectedPos, dTransform->tz, E, L"Should have been unchanged since no Z solving should take place");
        Assert::AreEqual(expectedVel, *dVel, E, L"Should be unchanged");
        return *this;
      }

      const TestObject& assertXYSolved() const {
        Assert::IsTrue(glm::length(getPos()) > 0.0f);
        Assert::IsTrue(glm::length(getVelocity()) > 0.0f);
        return *this;
      }

      Transform::PackedTransform* dTransform{};
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
        dy.dTransform->tz = 10.0f*dir;
        *dy.dVel = -20.0f*dir;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 0.0f, -10.0f*dir);
      }

      //Speed to exactly hit the ground
      for(float dir : POS_AND_NEG) {
        dy.dTransform->tz = 10.0f*dir;
        *dy.dVel = -10.0f*dir;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 0.0f, -10.0f*dir);
      }

      //Exactly touching
      for(float dir : POS_AND_NEG) {
        dy.reset();
        dy.dTransform->tz = 0.0f;
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
        dy.dTransform->tz = 10.0f;
        *dy.dVel = -20.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 1.0f, -9.0f);
      }
      //Speed to go up through the ground
      {
        dy.dTransform->tz = -10.0f;
        *dy.dVel = 20.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 0.0f, 10.0f);
      }

      //Speed to exactly hit the ground downward
      {
        dy.dTransform->tz = 10.0f;
        *dy.dVel = -9.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 1.0f, -9.0f);
      }
      //Speed to exactly hit the ground upward
      {
        dy.dTransform->tz = -10.0f;
        *dy.dVel = 10.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, 0.0f, 10.0f);
      }

      //Exactly touching edges and middle
      for(float pos : { 0.0f, 1.0f, 0.5f }) {
        for(float dir : POS_AND_NEG) {
          dy.reset();

          dy.dTransform->tz = pos;
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
        dy.dTransform->tz = 10.0f;
        *dy.dVel = -20.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, tb, -10.0f + tb);
      }
      //Speed to go up through the ground
      {
        dy.dTransform->tz = -10.0f;
        *dy.dVel = 20.0f;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, -ta, 10.0f - ta);
      }

      //Speed to exactly hit the ground downward
      {
        dy.dTransform->tz = 10.0f;
        *dy.dVel = -10.0f + tb;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, tb, -10.0f + tb);
      }
      //Speed to exactly hit the ground upward
      {
        dy.dTransform->tz = -10.0f;
        *dy.dVel = 10.0f - ta;

        game.game.update();

        dy.assertCollisionAtZWithVelocity(game.game, -ta, 10.0f - ta);
      }

      //Exactly touching edges and middle
      for(float pos : { 0.0f, tb, -ta }) {
        for(float dir : POS_AND_NEG) {
          dy.reset();

          dy.dTransform->tz = pos;
          *dy.dVel = dir;

          game.game.update();

          dy.assertZUnimpeded(pos + dir, dir).assertXYSolved();
        }
      }
    }
  };
}