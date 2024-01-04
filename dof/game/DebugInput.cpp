#include "Precompile.h"
#include "DebugInput.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "AppBuilder.h"
#include "GameInput.h"

namespace DebugInput {
  void updateDebugCamera(IAppBuilder& builder) {
    auto task = builder.createTask();
    const Config::GameConfig* config = TableAdapters::getGameConfig(task);
    auto query = task.query<
      const GameInput::StateMachineRow,
      Row<Camera>
    >();
    task.setCallback([query, config](AppTaskArgs&) mutable {
      query.forEachElement([config](const Input::StateMachine& sm, Camera& camera) {
        for(const Input::Event& event : sm.readEvents()) {
          switch(event.id) {
            case GameInput::Events::DEBUG_ZOOM: {
              const float speed = config->camera.cameraZoomSpeed;
              camera.zoom = std::max(0.0f, camera.zoom + sm.getAbsoluteAxis1D(event.toNode) * speed);
              break;
            }
          }
        }
      });
    });
    task.setName("debug camera");
    builder.submitTask(std::move(task));
  }
}