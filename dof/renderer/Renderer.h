#pragma once

#include "Simulation.h"
#include "DebugLinePassTable.h"
#include "glm/mat4x4.hpp"

class IAppBuilder;
class RuntimeDatabaseTaskBuilder;
struct StableElementMappings;
struct IDatabase;
struct sg_swapchain;
struct RuntimeDatabaseArgs;

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
  bool hasChanged{};
};

struct FONScontext;

struct RendererContext {
  const sg_swapchain& swapchain;
  FONScontext* fontContext{};
};

namespace Renderer {
  struct ICameraReader {
    virtual ~ICameraReader() = default;
    virtual void getAll(std::vector<RendererCamera>& out) = 0;
    virtual WindowData getWindow() = 0;
  };

  //Creates the renderer database using information from the game database
  void createDatabase(RuntimeDatabaseArgs& args);
  //Called after creating the database and a window has been created
  void init(IAppBuilder& builder, const RendererContext& context);
  void extractRenderables(IAppBuilder& builder);
  void clearRenderRequests(IAppBuilder& builder);
  void render(IAppBuilder& builder);
  void endMainPass(IAppBuilder& builder);
  void commit(IAppBuilder& builder);

  void preProcessEvents(IAppBuilder& builder);

  void injectRenderDependency(RuntimeDatabaseTaskBuilder& task);

  std::shared_ptr<ICameraReader> createCameraReader(RuntimeDatabaseTaskBuilder& task);
};