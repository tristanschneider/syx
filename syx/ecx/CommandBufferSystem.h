#pragma once

#include "CommandBuffer.h"
#include "LinearEntityRegistry.h"
#include "System.h"

namespace ecx {
  class CommandBuffersProvider {
  public:
    using EachCallback = std::function<void(CommandBuffer<LinearEntity>&)>;
    using Provider = std::function<void(const EachCallback&)>;

    CommandBuffersProvider() = default;
    CommandBuffersProvider(Provider provider)
      : mProvider(std::move(provider)) {
    }
    CommandBuffersProvider(CommandBuffersProvider&&) noexcept = default;
    CommandBuffersProvider(const CommandBuffersProvider&) noexcept = default;
    CommandBuffersProvider& operator=(const CommandBuffersProvider&) noexcept = default;
    CommandBuffersProvider& operator=(CommandBuffersProvider&&) noexcept = default;

    void foreachCommandBuffer(const EachCallback& callback) {
      if(mProvider) {
        mProvider(callback);
      }
    }

  private:
    Provider mProvider;
  };

  //No-op by default
  template<class EntityT, class... ToProcess>
  struct ProcessCommandBufferSystem : ISystem<EntityT> {
    void tick(EntityRegistry<EntityT>&, ThreadLocalContext&) const override {
    }

    SystemInfo<EntityT> getInfo() const override {
      return {};
    }
  };

  template<class... ToProcess>
  struct ProcessCommandBufferSystem<LinearEntity, ToProcess...> : ISystem<LinearEntity> {
    void tick(EntityRegistry<LinearEntity>& registry, ThreadLocalContext& localContext) const override {
      auto& provider = localContext.getOrCreate<CommandBuffersProvider>();
      provider.foreachCommandBuffer([&registry](CommandBuffer<LinearEntity>& buffer) {
        (buffer.processCommandsForComponent<ToProcess>(registry), ...);
      });
    }

    SystemInfo<LinearEntity> getInfo() const override {
      SystemInfo<LinearEntity> info;
      //Used to make functionality blocking
      info.mIsBlocking = true;
      info.mIsCommandProcessor = true;
      info.mName = "ProcessCommandSystem";
      return info;
    }
  };

  //No-op by default
  template<class EntityT>
  struct ProcessEntireCommandBufferSystem : ISystem<EntityT> {
    void tick(EntityRegistry<EntityT>&, ThreadLocalContext&) const override {
    }

    SystemInfo<EntityT> getInfo() const override {
      return {};
    }
  };

  template<>
  struct ProcessEntireCommandBufferSystem<LinearEntity> : ISystem<LinearEntity> {
    void tick(EntityRegistry<LinearEntity>& registry, ThreadLocalContext& localContext) const override {
      auto& provider = localContext.getOrCreate<CommandBuffersProvider>();
      provider.foreachCommandBuffer([&registry](CommandBuffer<LinearEntity>& buffer) {
        buffer.processAllCommands(registry);
        buffer.clear();
      });
    }

    SystemInfo<LinearEntity> getInfo() const override {
      SystemInfo<LinearEntity> info;
      //Used to make functionality blocking
      info.mIsBlocking = true;
      info.mIsCommandProcessor = true;
      info.mName = "ProcessAllCommandSystem";
      return info;
    }
  };
}
