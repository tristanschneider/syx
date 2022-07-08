#include "Precompile.h"

#include "ecs/component/GlobalCommandBufferComponent.h"
#include "ecs/system/GlobalCommandBufferSystem.h"

namespace GlobalCmd {
  using namespace Engine;
  struct BlockingSystem : System {
    ecx::SystemInfo getInfo() const override {
      ecx::SystemInfo info;
      info.mIsBlocking = true;
      return info;
    }
  };

  struct Init : BlockingSystem {
    void tick(EntityRegistry& registry, ecx::ThreadLocalContext&) const override {
      auto gen = registry.createEntityGenerator();
      auto globalEntity = registry.createEntity(*gen);
      registry.addComponent<GlobalCommandBufferComponent>(globalEntity, std::make_unique<ecx::CommandBuffer<Engine::Entity>>(std::move(gen)));
    }
  };
}

std::shared_ptr<Engine::System> GlobalCommandBufferSystem::init() {
  return std::make_shared<GlobalCmd::Init>();
}

std::shared_ptr<Engine::System> GlobalCommandBufferSystem::processCommands() {
  using namespace Engine;
  using GlobalView = View<GlobalCommandBufferComponent>;
  struct Sys : GlobalCmd::BlockingSystem {
    using GlobalView = View<Write<GlobalCommandBufferComponent>>;
    void tick(EntityRegistry& registry, ecx::ThreadLocalContext& localContext) const override {
      //Create the context to recycle the view like normal systems
      SystemContext<GlobalView> context(registry, localContext);
      for(auto cmd : context.get<GlobalView>()) {
        cmd.get<GlobalCommandBufferComponent>().mBuffer->processAllCommands(registry);
      }
    }
  };
  return std::make_shared<Sys>();
}
