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
  Renderable(Handle owner, MessageQueueProvider& messaging);

  const RenderableData& get() const;
  void set(const RenderableData& data);

private:
  void _fireUpdate();

  RenderableData mData;
};