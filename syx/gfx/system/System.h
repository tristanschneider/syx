#pragma once

class App;

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
  virtual void update(float dt) {}
  virtual void uninit() {}

protected:
  App* mApp;
};