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

private:
  std::unique_ptr<Lua::Node> _buildLuaProps() const;

  RenderableData mData;
};