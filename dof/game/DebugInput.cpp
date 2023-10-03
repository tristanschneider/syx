#include "Precompile.h"
#include "DebugInput.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "AppBuilder.h"

namespace DebugInput {
  void updateDebugCamera(IAppBuilder& builder) {
    auto task = builder.createTask();
    const Config::GameConfig* config = TableAdapters::getGameConfig(task);
    auto query = task.query<
      Row<DebugCameraControl>,
      Row<Camera>
    >();
    task.setCallback([query, config](AppTaskArgs&) mutable {
      query.forEachElement([config](DebugCameraControl& input, Camera& camera) {
        const float speed = config->camera.cameraZoomSpeed;
        camera.zoom = std::max(0.0f, camera.zoom + input.mAdjustZoom * speed);
        input.mLoadSnapshot = false;
        input.mTakeSnapshot = false;
      });
    });
    task.setName("debug camera");
    builder.submitTask(std::move(task));
  }
}