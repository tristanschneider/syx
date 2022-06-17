#pragma once

#include "CommandBuffer.h"
#include "LinearEntityRegistry.h"
#include "System.h"

namespace ecx {
  //No-op by default
  template<class EntityT, class... ToProcess>
  struct ProcessCommandBufferSystem : ISystem<EntityT> {
    void tick(EntityRegistry<EntityT>&, ThreadLocalContext&) const override {
    }

    SystemInfo getInfo() const override {
      return {};
    }
  };

  template<class... ToProcess>
  struct ProcessCommandBufferSystem<LinearEntity, ToProcess...> : ISystem<LinearEntity> {
    void tick(EntityRegistry<LinearEntity>& registry, ThreadLocalContext& localContext) const override {
      auto& buffer = localContext.getOrCreate<CommandBuffer<LinearEntity>>();
      (buffer.processCommandsForComponent<ToProcess>(registry), ...);
    }

    SystemInfo getInfo() const override {
      SystemInfo info;
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

    SystemInfo getInfo() const override {
      return {};
    }
  };

  template<>
  struct ProcessEntireCommandBufferSystem<LinearEntity> : ISystem<LinearEntity> {
    void tick(EntityRegistry<LinearEntity>& registry, ThreadLocalContext& localContext) const override {
      auto& buffer = localContext.getOrCreate<CommandBuffer<LinearEntity>>();
      buffer.processAllCommands(registry);
      buffer.clear();
    }

    SystemInfo getInfo() const override {
      SystemInfo info;
      //Used to make functionality blocking
      info.mIsBlocking = true;
      info.mIsCommandProcessor = true;
      info.mName = "ProcessAllCommandSystem";
      return info;
    }
  };
}
