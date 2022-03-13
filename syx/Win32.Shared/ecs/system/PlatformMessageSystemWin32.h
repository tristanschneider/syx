#pragma once

#include "ecs/ECS.h"
#include "ecs/component/PlatformMessageComponents.h"

//This is the glue between the platform thread and the rest of the game
//Platform pushes events to this which is responsible for ensuring threadsafe queueing,
//then the tick adds all queued messages to the game's ECS
struct PlatformMessageSystemWin32 {
  static std::shared_ptr<Engine::System> applyQueuedMessages();

  static void enqueueMessage(AllPlatformMessagesVariant message);
};