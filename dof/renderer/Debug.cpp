#include "Precompile.h"
#include "Debug.h"

DebugRenderData Debug::init() {
  DebugRenderData result{};
  return result;
}

void Debug::pictureInPicture(const DebugRenderData& data, const glm::vec2& min, const glm::vec2& max, sg_image texture) {
  data,min,max,texture;
}
