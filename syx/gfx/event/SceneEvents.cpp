#include "Precompile.h"
#include "event/SceneEvents.h"

DEFINE_EVENT(RegisterSceneEvent, Handle sceneId)
  , mSceneId(sceneId) {
}

DEFINE_EVENT(ClearSceneEvent, Handle sceneId)
  , mSceneId(sceneId) {
}