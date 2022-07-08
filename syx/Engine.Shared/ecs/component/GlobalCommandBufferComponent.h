#pragma once
#include "ecs/ECS.h"

struct GlobalCommandBufferSystem;

//Global buffer for deferring commands to a particular point in the frame as opposed to the local command buffers
class GlobalCommandBufferComponent {
public:
  friend struct GlobalCommandBufferSystem;
  using InternalBuffer = std::unique_ptr<ecx::CommandBuffer<Engine::Entity>>;

  GlobalCommandBufferComponent() = default;
  GlobalCommandBufferComponent(InternalBuffer buffer)
    : mBuffer(std::move(buffer)) {
  }
  GlobalCommandBufferComponent(GlobalCommandBufferComponent&&) noexcept = default;
  GlobalCommandBufferComponent& operator=(GlobalCommandBufferComponent&&) noexcept = default;

  template<class... Args>
  auto get() {
    return Engine::CommandBuffer<Args...>(*mBuffer);
  }

private:
  InternalBuffer mBuffer;
};