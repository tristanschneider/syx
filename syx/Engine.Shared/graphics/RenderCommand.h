#pragma once
#include "event/Event.h"

struct RenderCommand {
  enum class Type : uint8_t {
    Outline,
    Quad2d,
  };

  enum class Space : uint8_t {
    // Screen space top left origin
    ScreenPixel,
  };

  static RenderCommand outline(Handle obj, const Syx::Vec3& color, float width);
  static RenderCommand quad2d(const Syx::Vec2& min, const Syx::Vec2& max, const Syx::Vec3& color, Space space);

  Type mType;
  union {
    struct {
      float mColor[4];
      Handle mHandle;
      float mWidth;
    } mOutline;

    struct {
      float mMin[2];
      float mMax[2];
      float mColor[4];
      Space mSpace;
    } mQuad2d;
  };
};


class RenderCommandEvent : public TypedEvent<RenderCommandEvent> {
public:
  RenderCommandEvent(const RenderCommand& cmd);

  RenderCommand mCmd;
};

struct DispatchToRenderThreadEvent : public TypedEvent<DispatchToRenderThreadEvent> {
  DispatchToRenderThreadEvent(std::function<void()> callback)
    : mCallback(std::move(callback)) {
  }

  std::function<void()> mCallback;
};