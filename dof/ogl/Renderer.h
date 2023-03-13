#pragma once

#include "Simulation.h"
#include <Windows.h>
#include "GL/glew.h"

#include "Shader.h"

struct QuadUniforms {
  TextureSamplerUniform posX, posY, rotX, rotY, uv;
  GLuint worldToView;
  GLuint texture;
};

struct DebugDrawer {
  GLuint mShader{};
  GLuint mVBO{};
  GLuint mVAO{};
  GLuint mWVPUniform{};
  size_t mLastSize{};
};

struct OGLState {
  HGLRC mGLContext{};
  HDC mDeviceContext{};
  GLuint mQuadShader{};
  GLuint mQuadVertexBuffer{};
  QuadUniforms mQuadUniforms;
  DebugDrawer mDebug;
};

struct WindowData {
  HWND mWindow{};
  int mWidth{};
  int mHeight{};
  bool mFocused{};
};

struct TextureGLHandle {
  GLuint mHandle{};
};

struct TextureGameHandle {
  size_t mID = 0;
};

using TexturesTable = Table<
  Row<TextureGLHandle>,
  Row<TextureGameHandle>
>;

using GraphicsContext = Table<
  Row<OGLState>,
  Row<WindowData>
>;


using RendererDatabase = Database<
  GraphicsContext,
  TexturesTable
>;

struct Renderer {
  static void initDeviceContext(GraphicsContext::ElementRef& context);
  static void render(GameDatabase& db, RendererDatabase& renderDB);
};