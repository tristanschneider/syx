#pragma once

#include "AppBuilder.h"
#include "Simulation.h"
#include <Windows.h>
#include "GL/glew.h"

#include "DebugLinePassTable.h"
#include "Quad.h"
#include "QuadPassTable.h"
#include "Shader.h"
#include "glm/mat4x4.hpp"

struct RenderDebugDrawer {
  GLuint mShader{};
  GLuint mVBO{};
  GLuint mVAO{};
  GLuint mWVPUniform{};
  size_t mLastSize{};
};

struct RendererCamera {
  glm::vec2 pos{};
  Camera camera;
  glm::mat4 worldToView;
};

struct OGLState {
  HGLRC mGLContext{};
  HDC mDeviceContext{};
  GLuint mQuadShader{};
  GLuint mQuadVertexBuffer{};
  //Could be table but the amount of cameras isn't worth it
  std::vector<RendererCamera> mCameras;
  SceneState mSceneState;
  RenderDebugDrawer mDebug;
};

struct WindowData {
  HWND mWindow{};
  int mWidth{};
  int mHeight{};
  bool mFocused{};
  float aspectRatio{};
};

struct TextureGLHandle {
  GLuint mHandle{};
};

struct TextureGameHandle {
  size_t mID = 0;
};

namespace Renderer {
  //Creates the renderer database using information from the game database
  std::unique_ptr<IDatabase> createDatabase(RuntimeDatabaseTaskBuilder&& builder, StableElementMappings& mappings);
  //Called after creating the database and a window has been created
  void init(IAppBuilder& builder, HWND window);
  void processRequests(IAppBuilder& builder);
  void extractRenderables(IAppBuilder& builder);
  void clearRenderRequests(IAppBuilder& builder);
  void render(IAppBuilder& builder);
  void swapBuffers(IAppBuilder& builder);
};