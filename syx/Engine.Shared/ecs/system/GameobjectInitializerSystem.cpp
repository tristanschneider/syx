#include "Precompile.h"

#include "ecs/component/GameobjectComponent.h"
#include "ecs/component/SpaceComponents.h"
#include "ecs/system/GameobjectInitializerSystem.h"
#include <random>

namespace Initializer {
  using namespace Engine;
  using UninitializedView = View<Include<GameobjectComponent>, Exclude<GameobjectInitializedComponent>,
    OptionalRead<NameTagComponent>,
    OptionalRead<SerializeIDComponent>,
    OptionalRead<InSpaceComponent>
  >;
  using DefaultSpaceView = View<Include<SpaceTagComponent>, Include<DefaultPlaySpaceComponent>>;
  using Commands = CommandBuffer<GameobjectInitializedComponent,
    NameTagComponent,
    SerializeIDComponent,
    InSpaceComponent
  >;

  uint32_t _generateRandom() {
    //Set seed to current time for serialize id generation
    static std::mt19937 mt(static_cast<uint32_t>(std::time(nullptr)));
    return static_cast<uint32_t>(mt());
  }

  void tickInit(SystemContext<UninitializedView, DefaultSpaceView, Commands>& context) {
    auto& view = context.get<UninitializedView>();
    if(view.begin() == view.end()) {
      return;
    }
    auto cmd = context.get<Commands>();
    auto space = context.get<DefaultSpaceView>().tryGetFirst();
    const Entity spaceEntity = space ? space->entity() : Entity();
    for(auto uninitialized : context.get<UninitializedView>()) {
      const Entity entity = uninitialized.entity();
      if(!uninitialized.tryGet<const NameTagComponent>()) {
        cmd.addComponent<NameTagComponent>(entity).mName = "New Object";
      }
      //Serialized id can be loaded from serialized objects or generated at random if it doesn't exist
      if(!uninitialized.tryGet<const SerializeIDComponent>()) {
        cmd.addComponent<SerializeIDComponent>(entity).mId = _generateRandom();
      }
      //Space can be set explicitly, otherwise it will be assigned to the default space
      if(!uninitialized.tryGet<const InSpaceComponent>()) {
        cmd.addComponent<InSpaceComponent>(entity).mSpace = spaceEntity;
      }
      cmd.addComponent<GameobjectInitializedComponent>(entity);
    }
  }
}

std::shared_ptr<Engine::System> GameobjectInitializerSystem::create() {
  return ecx::makeSystem("GameobjectInit", &Initializer::tickInit);
}
