#pragma once
#include "Component.h"
#include "event/Event.h"

struct RenderableData {
  size_t mModel;
  size_t mDiffTex;
};

class RenderableUpdateEvent : public Event {
public:
  RenderableUpdateEvent(const RenderableData& data, Handle obj);

  Handle mObj;
  RenderableData mData;
};

class Renderable : public TypedComponent<Renderable> {
public:
  Renderable(Handle owner);
  Renderable(const Renderable& other);

  const RenderableData& get() const;
  void set(const RenderableData& data);

  std::unique_ptr<Component> clone() const override;
  void set(const Component& component) override;

  virtual const Lua::Node* getLuaProps() const override;

  COMPONENT_LUA_INHERIT(Renderable);
  virtual void openLib(lua_State* l) const;
  virtual const ComponentTypeInfo& getTypeInfo() const;

private:
  std::unique_ptr<Lua::Node> _buildLuaProps() const;

  RenderableData mData;
};