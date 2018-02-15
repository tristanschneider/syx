#include "Precompile.h"
#include "system/LuaGameSystem.h"

RegisterSystemCPP(LuaGameSystem);

LuaGameObject::LuaGameObject(Handle h)
  : mHandle(h) {
}

Handle LuaGameObject::getHandle() const {
  return mHandle;
}

LuaGameSystem::LuaGameSystem(const SystemArgs& args)
  : System(args) {
}

LuaGameSystem::~LuaGameSystem() {
}

void LuaGameSystem::init() {
}

void LuaGameSystem::queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
}

void LuaGameSystem::uninit() {
}
