#include "Precompile.h"
#include "system/ImGuiSystem.h"

#include "event/EventBuffer.h"
#include "event/store/ScreenSizeStore.h"
#include "graphics/RenderCommand.h"
#include "ImGuiImpl.h"
#include "input/InputStore.h"
#include "provider/MessageQueueProvider.h"
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"
#include "util/ScratchPad.h"

IImGuiSystem::IImGuiSystem(const SystemArgs& args)
  : System(args, System::_typeId<IImGuiSystem>()) {
}

ImGuiSystem::ImGuiSystem(const SystemArgs& args)
  : IImGuiSystem(args) {
}

ImGuiSystem::~ImGuiSystem() = default;

void ImGuiSystem::init() {
  mImpl = Create::imGuiImpl();
  mScreenSizeStore = std::make_shared<ScreenSizeStore>();
  mInputStore = std::make_shared<InputStore>();

  mEventHandler = std::make_unique<EventHandler>();
  mScreenSizeStore->init(*mEventHandler);
  mInputStore->init(*mEventHandler);
}

void ImGuiSystem::queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  IImGuiImpl::getPad().update();

  pool.queueTask(frameTask->then(std::make_shared<FunctionTask>([this, dt] {
    mEventHandler->handleEvents(*mEventBuffer);
    //Dispatch this to the render thread. It means rendering will always be "off" by one frame, but since
    //it's always rendering the latest
    mArgs.mMessages->getMessageQueue()->push(DispatchToRenderThreadEvent([this, dt] {
      if(IImGuiImpl* impl = _getImpl()) {
        impl->render(dt, mScreenSizeStore->get().mSize);
        impl->updateInput(*mInputStore);
      }
    }));
  })));
}

void ImGuiSystem::uninit() {
  mImpl = nullptr;
}

IImGuiImpl* ImGuiSystem::_getImpl() {
  return mImpl.get();
}
