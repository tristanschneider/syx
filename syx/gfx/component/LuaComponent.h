#pragma once
#include "Component.h"
#include "event/Event.h"

namespace Lua {
  class Sandbox;
  class State;
}

class AddLuaComponentEvent : public Event {
public:
  AddLuaComponentEvent(size_t owner, size_t script);

  size_t mOwner;
  size_t mScript;
};

class RemoveLuaComponentEvent : public Event {
public:
  RemoveLuaComponentEvent(size_t owner, size_t script);

  size_t mOwner;
  size_t mScript;
};

class LuaComponent : public Component {
public:
  LuaComponent(Handle owner);
  ~LuaComponent();

  size_t getScript() const;
  void setScript(size_t script);

  //The script must be at the top of the stack
  void init(Lua::State& state);
  void update(Lua::State& state, float dt, int selfIndex);
  void uninit();
  bool needsInit() const;

private:
  size_t mScript;
  std::unique_ptr<Lua::Sandbox> mSandbox;
};