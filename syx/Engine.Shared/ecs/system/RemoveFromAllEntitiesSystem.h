#pragma once

#include "ecs/ECS.h"

template<class ViewT, class... ToRemove>
inline std::shared_ptr<Engine::System> removeFomAllEntitiesInView() {
  using namespace Engine;
  using Modifier = EntityModifier<ToRemove...>;
  return ecx::makeSystem("removeFromAllInView", [](SystemContext<Modifier, ViewT>& context) {
    ViewT& view = context.get<ViewT>();
    Modifier modifier = context.get<Modifier>();

    for(auto chunk : view.chunks()) {
      while(chunk.size()) {
        Entity entity = chunk.indexToEntity(size_t(0));
        //TODO: add support to chunk to remove multiple at once
        (modifier.removeComponent<ToRemove>(entity), ...);
      }
    }
  });
}