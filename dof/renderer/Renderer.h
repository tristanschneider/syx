#pragma once

#include "Simulation.h"
#include "DebugLinePassTable.h"
#include "glm/mat4x4.hpp"

class IRenderingModule;
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

  void injectRenderDependency(RuntimeDatabaseTaskBuilder& task);

  std::shared_ptr<ICameraReader> createCameraReader(RuntimeDatabaseTaskBuilder& task);

  std::unique_ptr<IRenderingModule> createModule(const RendererContext& context);
};