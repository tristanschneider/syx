#include "Precompile.h"
#include "system/ImGuiSystem.h"

#include "ImGuiImpl.h"
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
}

void ImGuiSystem::queueTasks(float, IWorkerPool&, std::shared_ptr<Task>) {
  IImGuiImpl::getPad().update();
}

void ImGuiSystem::uninit() {
  mImpl = nullptr;
}

IImGuiImpl* ImGuiSystem::_getImpl() {
  return mImpl.get();
}
