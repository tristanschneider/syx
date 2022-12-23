#pragma once

#include "Simulation.h"
#include <Windows.h>

struct OGLState {
  HGLRC mGLContext{};
  HDC mDeviceContext{};
};

struct WindowData {
  HWND mWindow{};
  int mWidth{};
  int mHeight{};
  bool mFocused{};
};

using GraphicsContext = Table<
  Row<OGLState>,
  Row<WindowData>
>;

using RendererDatabase = Database<
  GraphicsContext
>;

struct Renderer {
  static void initDeviceContext(GraphicsContext::ElementRef& context);
  static void render(GameDatabase& db, RendererDatabase& renderDB);
};