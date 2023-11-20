#pragma once
#include "DebugLinePassTable.h"
#include "QuadPassTable.h"
#include "glm/vec4.hpp"

struct RendererDB;
class RuntimeDatabaseTaskBuilder;

struct OGLState;
struct WindowData;

//Assumes only one window/context exists
struct RendererGlobalsAdapter {
  operator bool() const {
    return state && window;
  }

  OGLState* state{};
  WindowData* window{};
};

struct RendererTableAdapters {
  static RendererGlobalsAdapter getGlobals(RuntimeDatabaseTaskBuilder& task);
};