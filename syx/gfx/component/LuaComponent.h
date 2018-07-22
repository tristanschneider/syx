#pragma once
#include "Component.h"
#include "event/Event.h"

namespace Lua {
  class Sandbox;
  class State;
  class Variant;
}

struct lua_State;

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
  LuaComponent(const LuaComponent& other);
  ~LuaComponent();

  size_t getScript() const;
  void setScript(size_t script);

  std::unique_ptr<Component> clone() const override;
  void set(const Component& component) override;
  const Lua::Node* getLuaProps() const override;
  const ComponentTypeInfo& getTypeInfo() const override;
  void _setSubType(size_t subType) override;
  void onPropsUpdated() override;

  //The script must be at the top of the stack
  void init(Lua::State& state, int selfIndex);
  void update(Lua::State& state, float dt, int selfIndex);
  void uninit();
  bool needsInit() const;

private:
  bool _callFunc(lua_State* s, const char* funcName, int arguments, int returns) const;
  std::unique_ptr<Lua::Node> _buildLuaProps() const;
  void _readPropsFromLua(lua_State* s);
  void _writePropsToLua(lua_State* s);

  size_t mScript;
  std::unique_ptr<Lua::Sandbox> mSandbox;
  std::unique_ptr<Lua::Variant> mProps;
  bool mPropsNeedWriteToLua = false;
};