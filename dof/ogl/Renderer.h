#pragma once

#include "AppBuilder.h"
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
  glm::vec2 pos{};
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

namespace Renderer {
  void initDeviceContext(GraphicsContext::ElementRef& context);
  //Creates the renderer database using information from the game database
  std::unique_ptr<IDatabase> initGame(IAppBuilder& builder);
  void processRequests(IAppBuilder& builder);
  void extractRenderables(IAppBuilder& builder);
  TaskRange extractRenderables(const GameDatabase& db, RendererDatabase& renderDB);
  void clearRenderRequests(IAppBuilder& builder);
  void render(RendererDatabase& renderDB);
  void swapBuffers(RendererDatabase& renderDB);
};