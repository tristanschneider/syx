#pragma once
#include "Component.h"
#include "event/Event.h"

struct RenderableData {
  Handle mModel;
  Handle mDiffTex;
};

class RenderableUpdateEvent : public Event {
public:
  RenderableUpdateEvent(const RenderableData& data, Handle obj);
  Handle getHandle() const override;
  std::unique_ptr<Event> clone() const override;

  Handle mObj;
  RenderableData mData;
};

class Renderable : public Component {
public:
  Renderable(Handle owner, MessagingSystem& messaging);

  const RenderableData& get() const;
  void set(const RenderableData& data);

private:
  void _fireUpdate();

  RenderableData mData;
};