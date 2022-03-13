#include "Precompile.h"
#include "ecs/system/GraphicsSystemBase.h"

#include "ecs/component/ScreenSizeComponent.h"
#include "ecs/component/PlatformMessageComponents.h"

namespace GraphicsBaseImpl {
  using namespace Engine;

  void init(SystemContext<EntityFactory>& context) {
    EntityFactory factory = context.get<EntityFactory>();
    factory.createEntityWithComponents<ScreenSizeComponent>();
  }

  using ScreenSizeMessageView = View<Read<OnWindowResizeMessageComponent>>;
  using ScreenSizeCacheView = View<Write<ScreenSizeComponent>>;

  //Update the cached screen size upon any screen size messages being created
  void tickScreenSize(SystemContext<ScreenSizeMessageView, ScreenSizeCacheView>& context) {
    //Looks like n^2 but there should only be one ScreenSizeComponent
    //TODO: if there are multiple in a frame this may end on the wrong one
    for(auto it : context.get<ScreenSizeMessageView>()) {
      const auto& newSize = it.get<const OnWindowResizeMessageComponent>();
      for(auto cache : context.get<ScreenSizeCacheView>()) {
        cache.get<ScreenSizeComponent>().mScreenSize = newSize.mNewSize;
      }
    }
  }
}

std::shared_ptr<Engine::System> GraphicsSystemBase::init() {
  return ecx::makeSystem("GraphicsBaseInit", &GraphicsBaseImpl::init);
}

std::shared_ptr<Engine::System> GraphicsSystemBase::screenSizeListener() {
  return ecx::makeSystem("GraphicsBaseScreenSIze", &GraphicsBaseImpl::tickScreenSize);
}
