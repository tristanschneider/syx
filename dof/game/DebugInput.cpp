#include "Precompile.h"
#include "DebugInput.h"

#include "Simulation.h"
#include "TableAdapters.h"

namespace DebugInput {
  void _updateDebugCamera(GameDatabase& db, CameraTable& cameras) {
    PROFILE_SCOPE("simulation", "debugcamera");
    constexpr const char* snapshotFilename = "debug.snap";
    bool loadSnapshot = false;
    const Config::GameConfig* config = TableAdapters::getConfig({ db }).game;
    for(size_t i = 0; i < TableOperations::size(cameras); ++i) {
      DebugCameraControl& input = std::get<Row<DebugCameraControl>>(cameras.mRows).at(i);
      const float speed = config->camera.cameraZoomSpeed;
      float& zoom = std::get<Row<Camera>>(cameras.mRows).at(i).zoom;
      zoom = std::max(0.0f, zoom + input.mAdjustZoom * speed);
      loadSnapshot = loadSnapshot || input.mLoadSnapshot;
      input.mLoadSnapshot = false;
      if(input.mTakeSnapshot) {
        Simulation::writeSnapshot(db, snapshotFilename);
      }
      input.mTakeSnapshot = false;
    }
    if(loadSnapshot) {
      Simulation::loadFromSnapshot(db, snapshotFilename);
    }
  }

  TaskRange updateDebugCamera(GameDB db) {
    auto task = TaskNode::create([db](...) {
      _updateDebugCamera(db.db, std::get<CameraTable>(db.db.mTables));
    });
    return TaskBuilder::addEndSync(task);
  }
}