#include "Precompile.h"
#include "ecs/system/PlatformMessageSystemWin32.h"

#include "ecs/component/MessageComponent.h"

namespace PlatformMessagesImpl {
  struct MessageContext {
    std::vector<AllPlatformMessagesVariant> mMessages;
    std::vector<AllPlatformMessagesVariant> mToSwap;
    std::mutex mMutex;
  };

  MessageContext& getMessageContext() {
    static MessageContext context;
    return context;
  }

  using namespace Engine;

  template<class MessageT>
  void addMessageEntity(MessageT message, EntityFactory& factory) {
    auto&& [a, msg, b] = factory.createAndGetEntityWithComponents<MessageT, MessageComponent>();
    msg.get() = std::move(message);
  }

  void tickApply(SystemContext<EntityFactory>& context) {
    auto factory = context.get<EntityFactory>();
    MessageContext& msg = getMessageContext();
    //The only thread using mToSwap is this so only the swap needs to be locked
    {
      std::scoped_lock<std::mutex> lock(msg.mMutex);
      msg.mMessages.swap(msg.mToSwap);
    }
    //Turn each queued message into an entity with that message
    for(auto& message : msg.mToSwap) {
      std::visit([&factory](auto& msg) {
        addMessageEntity(std::move(msg), factory);
      }, message);
    }

    //Clear the list which will be swapped in next frame
    msg.mToSwap.clear();
  }
};

std::shared_ptr<Engine::System> PlatformMessageSystemWin32::applyQueuedMessages() {
  return ecx::makeSystem("ApplyPlatformMessages", &PlatformMessagesImpl::tickApply);
}

void PlatformMessageSystemWin32::enqueueMessage(AllPlatformMessagesVariant message) {
  using namespace PlatformMessagesImpl;
  MessageContext& msg = getMessageContext();
  std::scoped_lock<std::mutex> lock(msg.mMutex);
  msg.mMessages.push_back(std::move(message));
}
