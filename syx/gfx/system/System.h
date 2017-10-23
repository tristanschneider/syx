#pragma once

class App;
class IWorkerPool;
class Task;

enum class SystemId : uint8_t {
  Graphics,
  KeyboardInput,
  EditorNavigator,
  Messaging,
  Physics,
  Count
};

class System {
public:
  friend class App;

  virtual SystemId getId() const = 0;

  virtual void init() {}
  virtual void update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {}
  virtual void uninit() {}

protected:
  App* mApp;
};