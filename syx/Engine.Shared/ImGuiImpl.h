#pragma once

class ScratchPad;
class Shader;
class KeyboardInput;

struct IImGuiImpl {
  virtual ~IImGuiImpl() = default;

  virtual void updateInput(KeyboardInput& input) = 0;
  virtual void render(float dt, Syx::Vec2 display) = 0;

  //TODO: move ownership of this to ImGuiSystem
  static ScratchPad& getPad();
};

namespace Create {
  std::unique_ptr<IImGuiImpl> imGuiImpl();
}
