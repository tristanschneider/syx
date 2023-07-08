#pragma once
#include "DebugLinePassTable.h"
#include "QuadPassTable.h"
#include "glm/vec4.hpp"

struct RendererDB;

struct QuadPassAdapter {
  Row<float>* posX{};
  Row<float>* posY{};
  Row<float>* rotX{};
  Row<float>* rotY{};
  Row<float>* linVelX{};
  Row<float>* linVelY{};
  Row<float>* angVel{};
  Row<glm::vec4>* tint{};
  SharedRow<bool>* isImmobile{};
  QuadPassTable::UV* uvs{};
  SharedRow<size_t>* texture{};
  SharedRow<QuadPass>* pass{};
};

struct OGLState;
struct WindowData;

struct RendererGlobalsAdapter {
  OGLState* state{};
  WindowData* window{};
  size_t size{};
};

struct RenderDebugAdapter {
  DebugLinePassTable::Type* table{};
  DebugLinePassTable::Points* points{};
  size_t size{};
};

struct RendererTableAdapters {
  static QuadPassAdapter getQuadPass(QuadPassTable::Type& table);
  static RendererGlobalsAdapter getGlobals(RendererDB db);
  static RenderDebugAdapter getDebug(RendererDB db);
};