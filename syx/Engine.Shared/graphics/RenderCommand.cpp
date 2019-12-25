#include "Precompile.h"
#include "graphics/RenderCommand.h"

RenderCommand RenderCommand::outline(Handle obj, const Syx::Vec3& color, float width) {
  return { Type::Outline, color.x, color.y, color.z, color.w, obj, width };
}

RenderCommand RenderCommand::quad2d(const Syx::Vec2& min, const Syx::Vec2& max, const Syx::Vec3& color, Space space) {
  RenderCommand cmd = { Type::Quad2d };
  cmd.mQuad2d = { min.x, min.y, max.x, max.y, color.x, color.y, color.z, color.w, space };
  return cmd;
}

DEFINE_EVENT(RenderCommandEvent, const RenderCommand& cmd)
  , mCmd(cmd) {
}