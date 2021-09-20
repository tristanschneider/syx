#pragma once
#include "SyxVec2.h"

class InputStore;
class ScratchPad;
class Shader;

struct IImGuiImpl {
  virtual ~IImGuiImpl() = default;

  virtual void updateInput(const InputStore& input) = 0;
  virtual void render(float dt, Syx::Vec2 display) = 0;

  //TODO: move ownership of this to ImGuiSystem
  static ScratchPad& getPad();
};

namespace Create {
  std::unique_ptr<IImGuiImpl> imGuiImpl();
}
