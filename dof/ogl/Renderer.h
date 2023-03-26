#pragma once

#include "Simulation.h"
#include <Windows.h>
#include "GL/glew.h"

#include "Quad.h"
#include "Shader.h"

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
  //One pass for each viewEachRow of quads. Could be compile time
  std::vector<QuadPass> mQuadPasses;
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
  static void initGame(GameDatabase& db, RendererDatabase& renderDB);
  static void render(GameDatabase& db, RendererDatabase& renderDB);
};