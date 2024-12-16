#pragma once

#include "Simulation.h"
#include "DebugLinePassTable.h"
#include "glm/mat4x4.hpp"

class IAppBuilder;
class RuntimeDatabaseTaskBuilder;
struct StableElementMappings;
struct IDatabase;
struct sg_swapchain;

struct RendererCamera {
  glm::vec2 pos{};
  Camera camera;
  glm::mat4 worldToView;
};

struct WindowData {
  int mWidth{};
  int mHeight{};
  bool mFocused{};
  float aspectRatio{};
};

//TODO: replace with ElementRef
struct TextureGameHandle {
  size_t mID = 0;
};

namespace Renderer {
  //Creates the renderer database using information from the game database
  std::unique_ptr<IDatabase> createDatabase(RuntimeDatabaseTaskBuilder&& builder, StableElementMappings& mappings);
  //Called after creating the database and a window has been created
  void init(IAppBuilder& builder, const sg_swapchain& swapchain);
  void processRequests(IAppBuilder& builder);
  void extractRenderables(IAppBuilder& builder);
  void clearRenderRequests(IAppBuilder& builder);
  void render(IAppBuilder& builder);
  void commit(IAppBuilder& builder);
};