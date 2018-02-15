#pragma once
#include "System.h"

class Task;

class LuaGameObject {
public:
  LuaGameObject(Handle h);
  Handle getHandle() const;

private:
  Handle mHandle;
};

class LuaGameSystem : public System {
public:
  RegisterSystemH(LuaGameSystem);
  LuaGameSystem(const SystemArgs& args);
  ~LuaGameSystem();

  void init() override;
  void queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
  void uninit() override;

private:
};