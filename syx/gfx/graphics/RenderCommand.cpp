#include "Precompile.h"
#include "graphics/RenderCommand.h"

RenderCommand RenderCommand::outline(Handle obj, const Syx::Vec3& color, float width) {
  return { Type::Outline, color, obj, width };
}

DEFINE_EVENT(RenderCommandEvent, const RenderCommand& cmd)
  , mCmd(cmd) {
}