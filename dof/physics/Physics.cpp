#include "Precompile.h"
#include "Physics.h"

#include "glm/common.hpp"
#include "out_ispc/unity.h"

#include "NotIspc.h"
#include "Profile.h"
#include <IAppModule.h>
#include <shapes/Mesh.h>

namespace Physics {
  std::unique_ptr<IAppModule> createModule(const PhysicsAliases&) {
    std::vector<std::unique_ptr<IAppModule>> modules;
    modules.push_back(Shapes::createMeshModule());
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

  void integrateVelocity(IAppBuilder& builder, const PhysicsAliases& aliases) {
    //Misleading name but the math is the same, add acceleration to velocity
    integratePositionAxis(builder, aliases.linVelX, ConstFloatQueryAlias::create<const AccelerationX>());
    integratePositionAxis(builder, aliases.linVelY, ConstFloatQueryAlias::create<const AccelerationY>());
    integratePositionAxis(builder, aliases.linVelZ, ConstFloatQueryAlias::create<const AccelerationZ>());
  }

  void integratePosition(IAppBuilder& builder, const PhysicsAliases& aliases) {
    integratePositionAxis(builder, aliases.posX, aliases.linVelX.read());
    integratePositionAxis(builder, aliases.posY, aliases.linVelY.read());
    integratePositionAxis(builder, aliases.posZ, aliases.linVelZ.read());
  }

  void integrateRotation(IAppBuilder& builder, const PhysicsAliases& aliases) {
    for(const TableID& table : builder.queryAliasTables(aliases.rotX, aliases.rotY, aliases.angVel.read())) {
      auto task = builder.createTask();
      auto query = task.queryAlias(table, aliases.rotX, aliases.rotY, aliases.angVel.read());
      task.setName("integrate rotation");
      task.setCallback([query](AppTaskArgs&) mutable {
        _integrateRotation(
          query.get<0>(0).data(),
          query.get<1>(0).data(),
          query.get<2>(0).data(),
          query.get<0>(0).size());
      });
      builder.submitTask(std::move(task));
    }
  }

  void applyDampingMultiplier(IAppBuilder& builder, const PhysicsAliases& aliases, const float& linearMultiplier, const float& angularMultiplier) {
    applyDampingMultiplierAxis(builder, aliases.linVelX, linearMultiplier);
    applyDampingMultiplierAxis(builder, aliases.linVelY, linearMultiplier);
    //Damping on Z doesn't really matter because the primary use case is simple upwards impulses counteracted by gravity
    applyDampingMultiplierAxis(builder, aliases.angVel, angularMultiplier);
  }
}