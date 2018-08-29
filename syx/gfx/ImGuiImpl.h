#pragma once

class Shader;
class KeyboardInput;

class ImGuiImpl {
public:
  ImGuiImpl();
  ~ImGuiImpl();

  void updateInput(KeyboardInput& input);
  void render(float dt, Syx::Vec2 display);

  static bool enabled() {
    return sEnabled;
  }

private:
  static bool sEnabled;

  void _initKeyMap();

  std::unique_ptr<Shader> mShader;
  GLHandle mVB;
  GLHandle mVA;
  GLHandle mIB;
  GLHandle mFontTexture;
};