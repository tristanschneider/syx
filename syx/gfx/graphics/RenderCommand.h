#pragma once
#include <event/Event.h>

struct RenderCommand {
  enum Type {
    Outline,
  };

  static RenderCommand outline(Handle obj, const Syx::Vec3& color, float width);

  Type mType;
  union {
    struct {
      Syx::Vec3 mColor;
      Handle mHandle;
      float mWidth;
    } mOutline;
  };
};


class RenderCommandEvent : public Event {
public:
  RenderCommandEvent(const RenderCommand& cmd);

  RenderCommand mCmd;
};