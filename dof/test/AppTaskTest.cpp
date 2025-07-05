#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppBuilder.h"
#include "SceneNavigator.h"
#include "TestGame.h"
#include "ThreadLocals.h"
#include "GameDatabase.h"
#include "ConstraintSolver.h"
#include "Geometric.h"
#include "stat/ConstraintStatEffect.h"
#include "TransformResolver.h"
#include "PhysicsSimulation.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(AppTaskTest) {
    struct PhysicsRows {
      PhysicsRows(RuntimeTable& table)
        : stable{ table.tryGet<StableIDRow>() }
        , posX{ table.tryGet<Tags::PosXRow>() }
        , posY{ table.tryGet<Tags::PosYRow>() }
        , linVelX{ table.tryGet<Tags::LinVelXRow>() }
        , mass{ table.tryGet<ConstraintSolver::MassRow>() }
      {
      }

      StableIDRow* stable{};
      Tags::PosXRow* posX{};
      Tags::PosYRow* posY{};
      Tags::LinVelXRow* linVelX{};
      ConstraintSolver::MassRow* mass{};
    };

    struct Indices {
      Indices(size_t base)
        : d{ base }
        , s{ base + 1 }
        , ca{ base + 2 }
        , cb{ base + 3 }
      {
      }
      const size_t d;
      const size_t s;
      const size_t ca;
      const size_t cb;
    };

    struct PhysicsLocalDBTask : SceneNavigator::IScene {
      std::array<ElementRef*, 4> getHandleArray() {
        return { &dynamicObj, &staticObj, &constraintObjA, &constraintObjB };
      }

      void init(IAppBuilder& builder) final {
        auto task = builder.createTask();
        GameDatabase::Tables tables{ task };
        ElementRefResolver res = task.getIDResolver()->getRefResolver();

        task.setCallback([=](AppTaskArgs& args) {
          RuntimeDatabase& db = args.getLocalDB();
          RuntimeTable* table = db.tryGet(tables.physicsObjsWithZ);
          db.setTableDirty(tables.physicsObjsWithZ);

          Assert::IsNotNull(table);
          PhysicsRows pr{ *table };
          //Create the objects
          const Indices i = table->addElements(4);
          auto objs = { i.d, i.s, i.ca, i.cb };
          auto handles = getHandleArray();
          //Gather ElementRefs for the new objects
          {
            auto co = objs.begin();
            auto ch = handles.begin();
            for(; co != objs.end(); ++co, ++ch) {
              **ch = pr.stable->at(*co);
            }
          }

          Assert::IsFalse(res.tryUnpack(dynamicObj).has_value(), L"Refs shouldn't be accessible in the main DB until they are migrated there");

          //Compute masses
          const Mass::OriginMass regularMass = Mass::computeQuadMass(Mass::Quad{ .fullSize = glm::vec2{ 1.f } }).body;
          const Mass::OriginMass zeroMass{};
          for(size_t e : objs) {
            pr.mass->at(e) = regularMass;
          }
          pr.mass->at(i.s) = zeroMass;

          //Position dynamic object going towards static one
          pr.posX->at(i.s) = 1;
          pr.posY->at(i.s) = pr.posY->at(i.d) = 1;
          pr.posX->at(i.d) = -1;
          pr.linVelX->at(i.d) = 0.5f;

          //Position constraint pair away from the above
          pr.posY->at(i.ca) = pr.posY->at(i.cb) = -5;
          pr.posX->at(i.ca) = 5;
          pr.posX->at(i.cb) = 7;

          ConstraintStatEffect::Builder builder{ args };
          builder.createStatEffects(1).setLifetime(StatEffect::INFINITE);
          builder.constraintBuilder().setJointType({ Constraints::WeldJoint{
            .localCenterToPinA = { 1, 0 },
            .localCenterToPinB = { -1, 0 },
            .allowedRotationRad = 0
          }}).setTargets(constraintObjA, constraintObjB);
        });

        builder.submitTask(std::move(task.setName("t")));
      }

      void update(IAppBuilder& builder) final {
        auto task = builder.createTask();
        pt::TransformResolver tr = PhysicsSimulation::createTransformResolver(task);

        task.setCallback([=](AppTaskArgs&) mutable {
          std::array<pt::Transform, 4> transforms;
          log.push_back(LogE{ transforms });
          const Indices i{ 0 };
          auto handles = getHandleArray();
          std::transform(handles.begin(), handles.end(), transforms.begin(), [&](ElementRef* e) {
            return tr.resolve(*e);
          });

          if(transforms[i.d].pos.x > transforms[i.s].pos.x) {
            Assert::Fail(L"Dynamic object should have collided with static object and stopped");
          }

          if(++currentTick > END_TICKS) {
            constexpr float e = 0.01f;
            Assert::AreEqual(transforms[i.s].pos.x, 1, e);
            Assert::AreEqual(transforms[i.s].pos.y, 1, e);
            Assert::IsTrue(transforms[i.d].pos.x > -1, L"Dynamic object should have moved due to velocity");

            const float dist = glm::distance(transforms[i.ca].pos, transforms[i.cb].pos);
            const float distError = std::abs(2.0f - dist);
            Assert::IsTrue(distError < e, L"Constraint should have moved the objects and stabilized");
            currentTick = 0;
          }
        });

        builder.submitTask(std::move(task.setName("a")));
      }

      static constexpr size_t END_TICKS = 200;

      struct LogE {
        std::array<pt::Transform, 4> transforms;
      };
      size_t currentTick{};
      std::vector<LogE> log;
      ElementRef dynamicObj, staticObj;
      ElementRef constraintObjA, constraintObjB;
    };

    TEST_METHOD(AppTask_CreatePhysicsObjects) {
      TestGame game{ std::make_unique<PhysicsLocalDBTask>() };
      for(size_t i = 0; i < PhysicsLocalDBTask::END_TICKS + 10; ++i) {
        game.update();
      }
    }
  };
}
