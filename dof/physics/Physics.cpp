#include "Precompile.h"
#include "Physics.h"

#include "glm/common.hpp"
#include "out_ispc/unity.h"

#include "NotIspc.h"
#include "Profile.h"
#include <IAppModule.h>
#include <shapes/Mesh.h>

#include <module/MassModule.h>
#include <module/PhysicsEvents.h>
#include <transform/TransformRows.h>
#include <TLSTaskImpl.h>

namespace Physics {
  std::unique_ptr<IAppModule> createModule() {
    std::vector<std::unique_ptr<IAppModule>> modules;
    modules.push_back(Shapes::createMeshModule());
    modules.push_back(MassModule::createModule());
    modules.push_back(PhysicsEvents::clearEvents());
    return std::make_unique<CompositeAppModule>(std::move(modules));
  }

  void _integratePositionAxis(const float* velocity, float* position, size_t count) {
    ispc::integratePosition(position, velocity, uint32_t(count));
  }

  void _integrateRotation(float* rotX, float* rotY, const float* velocity, size_t count) {
    ispc::integrateRotation(rotX, rotY, velocity, uint32_t(count));
  }

  void _applyDampingMultiplier(float* velocity, float amount, size_t count) {
    ispc::applyDampingMultiplier(velocity, amount, uint32_t(count));
  }

  void applyDampingMultiplierAxis(IAppBuilder& builder, const QueryAlias<Row<float>>& axis, const float& multiplier) {
    for(const TableID& table : builder.queryAliasTables(axis)) {
      auto task = builder.createTask();
      task.setName("damping");
      Row<float>* axisRow = &task.queryAlias(table, axis).get<0>(0);
      task.setCallback([axisRow, &multiplier](AppTaskArgs&) {
        _applyDampingMultiplier(axisRow->data(), multiplier, axisRow->size());
      });

      builder.submitTask(std::move(task));
    }
  }

  void integratePositionAxis(IAppBuilder& builder, const QueryAlias<Row<float>>& position, const QueryAlias<const Row<float>>& velocity) {
    for(const TableID& table : builder.queryAliasTables(position, velocity)) {
      auto task = builder.createTask();
      task.setName("Integrate Position");
      auto query = task.queryAlias(table, position, velocity.read());
      task.setCallback([query](AppTaskArgs&) mutable {
        _integratePositionAxis(
          query.get<1>(0).data(),
          query.get<0>(0).data(),
          query.get<0>(0).size()
        );
      });
      builder.submitTask(std::move(task));
    }
  }

  void integratePositionAxis(IAppBuilder& builder, float(Transform::PackedTransform::*axis), const QueryAlias<const Row<float>>& velocity) {
    const auto transformAlias = QueryAlias<Transform::WorldTransformRow>::create().write();
    for(const TableID& table : builder.queryAliasTables(transformAlias, velocity)) {
      auto task = builder.createTask();
      task.setName("Integrate Position");
      auto query = task.queryAlias(table, transformAlias, velocity.read());
      task.setCallback([query, axis](AppTaskArgs&) mutable {
        auto [transforms, velocities] = query.get(0);
        for(size_t i = 0; i < transforms->size(); ++i) {
          Transform::PackedTransform& t = transforms->at(i);
          t.*axis += velocities->at(i);
        }
      });
      builder.submitTask(std::move(task));
    }
  }

  void integrateVelocity(IAppBuilder& builder) {
    //Misleading name but the math is the same, add acceleration to velocity
    integratePositionAxis(builder, FloatQueryAlias::create<VelX>(), ConstFloatQueryAlias::create<const AccelX>());
    integratePositionAxis(builder, FloatQueryAlias::create<VelY>(), ConstFloatQueryAlias::create<const AccelY>());
    integratePositionAxis(builder, FloatQueryAlias::create<VelZ>(), ConstFloatQueryAlias::create<const AccelZ>());
  }

  struct Integrator {
    struct Group {
      void init(const TableID& _table) {
        table = _table;
      }

      void init(RuntimeDatabaseTaskBuilder& task) {
        integrateIndex = 0;
        int currentBit = 1;
        auto tryQuery = [&](const QueryAlias<const Row<float>>& alias) {
          QueryResult<const Row<float>> q = task.queryAlias(table, alias);
          const Row<float>* result{};
          if(q.size()) {
            result = &q.get<0>(0);
            integrateIndex |= currentBit;
          }
          currentBit <<= 1;
          return result;
        };
        angVel = tryQuery(ConstFloatQueryAlias::create<const VelA>());
        linVelZ = tryQuery(ConstFloatQueryAlias::create<const VelZ>());
        linVelY = tryQuery(ConstFloatQueryAlias::create<const VelY>());
        linVelX = tryQuery(ConstFloatQueryAlias::create<const VelX>());
        transformQuery = task.query<Transform::WorldTransformRow, Transform::TransformNeedsUpdateRow>(table);
      }

      struct Accumulator {
        float accumulate(float v) {
          total += std::abs(v);
          return v;
        }

        bool didMove() const {
          //Currently needs to be zero or transform updates will slowly slip off course.
          //This should be reevaluated when sleep is added.
          return total > 0; //Constants::EPSILON;
        }

        float total{};
      };

      void flagIfMoved(size_t i, Transform::TransformNeedsUpdateRow& needsUpdates, const Accumulator& a) const {
        if(a.didMove()) {
          needsUpdates.getOrAdd(i);
        }
      }

      template<bool X, bool Y, bool Z>
      void integrateLinearPart(size_t i, Transform::PackedTransform& t, Accumulator& a, float dt) {
        if constexpr(X) {
          t.tx += a.accumulate(linVelX->at(i)*dt);
        }
        if constexpr(Y) {
          t.ty += a.accumulate(linVelY->at(i)*dt);
        }
        if constexpr(Z) {
          t.tz += a.accumulate(linVelZ->at(i)*dt);
        }
      }

      template<bool X, bool Y, bool Z>
      void integrateLinear(float dt) {
        auto [transforms, needUpdates] = transformQuery.get(0);
        for(size_t i = 0; i < transforms->size(); ++i) {
          Accumulator a;
          integrateLinearPart<X, Y, Z>(i, transforms->at(i), a, dt);
          flagIfMoved(i, *needUpdates, a);
        }
      }

      template<bool X, bool Y, bool Z>
      void integrateLinearAndAngular(float dt) {
        auto [transforms, needUpdates] = transformQuery.get(0);
        for(size_t i = 0; i < transforms->size(); ++i) {
          Transform::PackedTransform& t = transforms->at(i);
          Accumulator a;

          integrateLinearPart<X, Y, Z>(i, t, a, dt);
          const float av = a.accumulate(angVel->at(i)*dt);
          //In theory this may deteriorate the scale over time due to accumulating float precision issues
          t = t.rotatedInPlace(av);

          flagIfMoved(i, *needUpdates, a);
        }
      }

      template<bool X, bool Y, bool Z, bool A>
      void integrate(float dt) {
        if constexpr(!A) {
          integrateLinear<X, Y, Z>(dt);
        }
        else {
          integrateLinearAndAngular<X, Y, Z>(dt);
        }
      }

      void execute() {
        (this->*(Integrators[integrateIndex]))(1.f);
      }

      static constexpr std::array<void(Group::*)(float), 16> Integrators = {
        &integrate<false, false, false, false>,
        &integrate<false, false, false, true>,
        &integrate<false, false, true, false>,
        &integrate<false, false, true, true>,
        &integrate<false, true, false, false>,
        &integrate<false, true, false, true>,
        &integrate<false, true, true, false>,
        &integrate<false, true, true, true>,
        &integrate<true, false, false, false>,
        &integrate<true, false, false, true>,
        &integrate<true, false, true, false>,
        &integrate<true, false, true, true>,
        &integrate<true, true, false, false>,
        &integrate<true, true, false, true>,
        &integrate<true, true, true, false>,
        &integrate<true, true, true, true>,
      };

      int integrateIndex{};
      QueryResult<Transform::WorldTransformRow, Transform::TransformNeedsUpdateRow> transformQuery;
      const Row<float>* linVelX{};
      const Row<float>* linVelY{};
      const Row<float>* linVelZ{};
      const Row<float>* angVel{};
      TableID table;
    };

    void init() {}

    void execute(Group& g) {
      g.execute();
    }
  };
  struct IntegrateTask {
    std::array<bool, 3> hasAxis{};
  };

  void integratePositionAndRotation(IAppBuilder& builder) {
    std::unordered_set<TableID> tables;
    const auto t = QueryAlias<Transform::WorldTransformRow>::create();
    auto addTables = [&](const QueryAlias<Row<float>>& alias) {
      for(const TableID& table : builder.queryAliasTables(t, alias)) {
        tables.insert(table);
      }
    };
    addTables(FloatQueryAlias::create<VelX>());
    addTables(FloatQueryAlias::create<VelY>());
    addTables(FloatQueryAlias::create<VelZ>());
    addTables(FloatQueryAlias::create<VelA>());

    for(const TableID& table : tables) {
      builder.submitTask(TLSTask::createWithArgs<Integrator, Integrator::Group>("IntegratePosition", table));
    }
  }

  void applyDampingMultiplier(IAppBuilder& builder, const float& linearMultiplier, const float& angularMultiplier) {
    applyDampingMultiplierAxis(builder, FloatQueryAlias::create<VelX>(), linearMultiplier);
    applyDampingMultiplierAxis(builder, FloatQueryAlias::create<VelY>(), linearMultiplier);
    //Damping on Z doesn't really matter because the primary use case is simple upwards impulses counteracted by gravity
    applyDampingMultiplierAxis(builder, FloatQueryAlias::create<VelA>(), angularMultiplier);
  }
}