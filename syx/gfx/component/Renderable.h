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

class Renderable : public Component {
public:
  Renderable(Handle owner);

  const RenderableData& get() const;
  void set(const RenderableData& data);

  virtual const Lua::Node* getLuaProps() const override;

  COMPONENT_LUA_INHERIT(CLASS_NAME);
  virtual void openLib(lua_State* l) const;
  //Name is the name the component appears as on the gameobject, while typename is the name of the lua type for the component
  virtual const std::string& getName() const;
  virtual const std::string& getTypeName() const;
  virtual size_t getNameConstHash() const;

private:
  static const std::string CLASS_NAME;
  static const std::pair<std::string, size_t> NAME_HASH;

  std::unique_ptr<Lua::Node> _buildLuaProps() const;

  RenderableData mData;
};