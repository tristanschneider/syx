#include "Precompile.h"
#include "ecs/system/physics/PhysicsSystem.h"

#include "ecs/component/PhysicsComponents.h"

namespace Impl {
  using namespace Tags;
  using namespace Engine;
  using FactoryCommands = CommandBuffer<
    CreatePhysicsObjectRequestComponent,
    ComputeMassRequestComponent,
    ComputeInertiaRequestComponent,
    PhysicsOwner,
    PhysicsObject,
    FloatComponent<Position, X>,
    FloatComponent<Position, Y>,
    FloatComponent<Position, Z>,
    FloatComponent<Rotation, I>,
    FloatComponent<Rotation, J>,
    FloatComponent<Rotation, K>,
    FloatComponent<Rotation, W>,
    FloatComponent<LinearVelocity, X>,
    FloatComponent<LinearVelocity, Y>,
    FloatComponent<LinearVelocity, Z>,
    FloatComponent<AngularVelocity, X>,
    FloatComponent<AngularVelocity, Y>,
    FloatComponent<AngularVelocity, Z>,
    FloatComponent<LocalInertia, X>,
    FloatComponent<LocalInertia, Y>,
    FloatComponent<LocalInertia, Z>,
    FloatComponent<Inertia, A>,
    FloatComponent<Inertia, B>,
    FloatComponent<Inertia, C>,
    FloatComponent<Inertia, D>,
    FloatComponent<Inertia, E>,
    FloatComponent<Inertia, F>,
    FloatComponent<Mass, Value>
  >;

  using CreateView = View<Include<CreatePhysicsObjectRequestComponent>>;
  void _createPhysicsObjects(SystemContext<CreateView, FactoryCommands>& context) {
    auto cmd = context.get<FactoryCommands>();
    for(auto&& chunk : context.get<CreateView>().chunks()) {
      for(size_t i = 0; i < chunk.size(); ++i) {
        const Entity owner = chunk.indexToEntity(i);
        auto tuple = cmd.createAndGetEntityWithComponents<
          PhysicsObject,
          ComputeMassRequestComponent,
          ComputeInertiaRequestComponent,
          FloatComponent<Position, X>,
          FloatComponent<Position, Y>,
          FloatComponent<Position, Z>,
          FloatComponent<Rotation, I>,
          FloatComponent<Rotation, J>,
          FloatComponent<Rotation, K>,
          FloatComponent<Rotation, W>,
          FloatComponent<LinearVelocity, X>,
          FloatComponent<LinearVelocity, Y>,
          FloatComponent<LinearVelocity, Z>,
          FloatComponent<AngularVelocity, X>,
          FloatComponent<AngularVelocity, Y>,
          FloatComponent<AngularVelocity, Z>,
          FloatComponent<LocalInertia, X>,
          FloatComponent<LocalInertia, Y>,
          FloatComponent<LocalInertia, Z>,
          FloatComponent<Inertia, A>,
          FloatComponent<Inertia, B>,
          FloatComponent<Inertia, C>,
          FloatComponent<Inertia, D>,
          FloatComponent<Inertia, E>,
          FloatComponent<Inertia, F>,
          FloatComponent<Mass, Value>
        >();

        //Link the new object to its owner
        const Entity physicsObject = std::get<0>(tuple);
        std::get<1>(tuple)->mPhysicsOwner = owner;

        //Link the owner to the new object
        cmd.addComponent<PhysicsOwner>(owner).mPhysicsObject = physicsObject;
        cmd.removeComponent<CreatePhysicsObjectRequestComponent>(owner);
      }
    }
  }
}

std::vector<std::shared_ptr<Engine::System>> PhysicsSystems::createDefault() {
  std::vector<std::shared_ptr<Engine::System>> systems;
  systems.push_back(ecx::makeSystem("CreatePhysicsObjects", &Impl::_createPhysicsObjects));
  return systems;
}
