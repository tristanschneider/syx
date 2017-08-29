#pragma once

class Shader;

class ImGuiImpl {
public:
  ImGuiImpl();
  ~ImGuiImpl();

  void render(float dt, Syx::Vec2 display);

  static bool enabled() {
    return sEnabled;
  }

private:
  static bool sEnabled;

  std::unique_ptr<Shader> mShader;
  GLuint mVB;
  GLuint mVA;
  GLuint mIB;
  GLuint mFontTexture;
};