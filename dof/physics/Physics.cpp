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
#include <loader/ReflectionModule.h>
#include <generics/Functional.h>
#include <TLSTaskImpl.h>
#include <Narrowphase.h>
#include <ConstraintSolver.h>
#include <SweepNPruneBroadphase.h>
#include <Constraints.h>
#include <shapes/DefaultShapes.h>

namespace Physics {
  struct VelocityLoader {
    using src_row = Loader::Vec4Row;
    static constexpr std::string_view NAME = "Velocity";

    static constexpr DBTypeID HASH = Loader::getDynamicRowKey<Loader::Vec4Row>("Velocity");

    static void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) {
      using namespace gnx::func;
      using namespace Reflection;

      const Loader::Vec4Row& s = static_cast<const Loader::Vec4Row&>(src);
      tryLoadRow<VelX>(s, dst, range, GetX{});
      tryLoadRow<VelY>(s, dst, range, GetY{});
      tryLoadRow<VelZ>(s, dst, range, GetZ{});
      tryLoadRow<VelA>(s, dst, range, GetW{});
    }
  };

  void initFromConfig(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("init physics from config");
    const Config::PhysicsConfig* config = &task.query<const SharedRow<Config::GameConfig>>().tryGetSingletonElement()->physics;
    auto query = task.query<SharedRow<Broadphase::SweepGrid::Grid>>();
    task.setCallback([query, config](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto& grid = query.get<0>(t).at();
        grid.definition.bottomLeft = { config->broadphase.bottomLeftX, config->broadphase.bottomLeftY };
        grid.definition.cellSize = { config->broadphase.cellSizeX, config->broadphase.cellSizeY };
        grid.definition.cellsX = config->broadphase.cellCountX;
        grid.definition.cellsY = config->broadphase.cellCountY;
        Broadphase::SweepGrid::init(grid);
      }
    });
    builder.submitTask(std::move(task));
  }

  void updatePhysics(IAppBuilder& builder, size_t threadCount) {
    auto temp = builder.createTask();
    const Config::PhysicsConfig& config = temp.query<SharedRow<Config::GameConfig>>().tryGetSingletonElement()->physics;

    //TODO: move to config
    static float biasTerm = ConstraintSolver::SolverGlobals::BIAS_DEFAULT;
    static float slop = ConstraintSolver::SolverGlobals::SLOP_DEFAULT;
    ConstraintSolver::SolverGlobals globals{
      &biasTerm,
      &slop
    };
    temp.discard();

    Physics::integrateVelocity(builder);
    Physics::applyDampingMultiplier(builder, config.linearDragMultiplier, config.angularDragMultiplier);
    SweepNPruneBroadphase::updateBroadphase(builder);
    Constraints::update(builder, globals);

    Narrowphase::generateContactsFromSpatialPairs(builder, threadCount);
    ConstraintSolver::solveConstraints(builder, globals);

    Physics::integratePositionAndRotation(builder);
  }

  class UpdateModule : public IAppModule {
  public:
    UpdateModule(std::function<size_t(RuntimeDatabaseTaskBuilder&)> _threadCount)
      : threadCount{ std::move(_threadCount) } {
    }

    void init(IAppBuilder& builder) final {
      using namespace Reflection;
      Reflection::registerLoaders(builder,
        createRowLoader(VelocityLoader{}),
        createRowLoader(VelocityLoader{}, "Velocity3D"),
        createRowLoader(DirectRowLoader<Loader::BitfieldRow, ConstraintSolver::ConstraintMaskRow>{}),
        createRowLoader(DirectRowLoader<Loader::BitfieldRow, Narrowphase::CollisionMaskRow>{})
      );
      initFromConfig(builder);

      auto task = builder.createTask();
      task.setName("init physics");
      auto query = task.query<SweepNPruneBroadphase::BroadphaseKeys>();
      task.setCallback([query](AppTaskArgs&) mutable {
        query.forEachRow([](auto& row) { row.setDefaultValue(Broadphase::SweepGrid::EMPTY_KEY); });
      });
      builder.submitTask(std::move(task));

      auto temp = builder.createTask();
      temp.discard();
      ShapeRegistry::IShapeRegistry* reg = ShapeRegistry::getMutable(temp);
      Shapes::registerDefaultShapes(*reg);
      ShapeRegistry::finalizeRegisteredShapes(builder);
    }

    void dependentInit(IAppBuilder& builder) final {
      //Dependent init because this requires users to have filled in their constraint definitions in their own inits
      Constraints::init(builder);
    }

    void update(IAppBuilder& builder) final {
      auto temp = builder.createTask();
      temp.discard();
      updatePhysics(builder, threadCount(temp));
    }

    void preProcessEvents(IAppBuilder& builder) final {
      SweepNPruneBroadphase::preProcessEvents(builder);
    }

    void postProcessEvents(IAppBuilder& builder) final {
      SweepNPruneBroadphase::postProcessEvents(builder);
      Constraints::postProcessEvents(builder);
    }

  private:
    std::function<size_t(RuntimeDatabaseTaskBuilder&)> threadCount;
  };

  std::unique_ptr<IAppModule> createModule(std::function<size_t(RuntimeDatabaseTaskBuilder&)> threadCount) {
    std::vector<std::unique_ptr<IAppModule>> modules;
    modules.push_back(std::make_unique<UpdateModule>(std::move(threadCount)));
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

  std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task) {
    return ShapeRegistry::get(task)->createShapeClassifier(task);
  }
}