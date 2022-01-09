#pragma once

#include "ecs/ECS.h"

//Remove all entities in view
template<class ViewT>
struct RemoveEntitiesSystem {
  static auto create() {
    return ecx::makeSystem("RemoveEntities", &_tick);
  }

  static void _tick(Engine::SystemContext<ViewT, Engine::EntityFactory>& context) {
    auto& view = context.get<ViewT>();
    auto factory = context.get<Engine::EntityFactory>();

    for(auto chunk = view.chunksBegin(); chunk != view.chunksEnd(); ++chunk) {
      (*chunk).clear(factory);
    }
  }
};