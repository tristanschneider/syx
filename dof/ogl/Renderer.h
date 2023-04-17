#pragma once

#include "Simulation.h"
#include <Windows.h>
#include "GL/glew.h"

#include "DebugLinePassTable.h"
#include "Quad.h"
#include "QuadPassTable.h"
#include "Shader.h"

struct DebugDrawer {
  GLuint mShader{};
  GLuint mVBO{};
  GLuint mVAO{};
  GLuint mWVPUniform{};
  size_t mLastSize{};
};

struct RendererCamera {
  Camera camera;
};

struct OGLState {
  HGLRC mGLContext{};
  HDC mDeviceContext{};
  GLuint mQuadShader{};
  GLuint mQuadVertexBuffer{};
  //One pass for each viewEachRow of quads. Could be compile time
  std::vector<QuadPassTable::Type> mQuadPasses;
  //Could be table but the amount of cameras isn't worth it
  std::vector<RendererCamera> mCameras;
  SceneState mSceneState;
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
  TexturesTable,
  DebugLinePassTable::Type
>;

struct RendererDB {
  RendererDatabase& db;
};

struct Renderer {
  static void initDeviceContext(GraphicsContext::ElementRef& context);
  static void initGame(GameDatabase& db, RendererDatabase& renderDB);
  static void processRequests(GameDatabase& db, RendererDatabase& renderDB);
  static TaskRange extractRenderables(const GameDatabase& db, RendererDatabase& renderDB);
  static TaskRange clearRenderRequests(GameDatabase& db);
  static void render(RendererDatabase& renderDB);
  static void swapBuffers(RendererDatabase& renderDB);
};