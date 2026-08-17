#include <sandbox/scene/Scene.h>

void sbx_Scene_dtor(sbx_SceneDtorArgs* args) {
  if(args && args->scene && args->scene->dtor) {
    args->scene->dtor(args);
  }
}

void sbx_Scene_init(sbx_SceneInitArgs* args) {
  if(args && args->scene && args->scene->init) {
    args->scene->init(args);
  }
}

void sbx_Scene_frame(sbx_SceneFrameArgs* args) {
  if(args && args->scene && args->scene->frame) {
    args->scene->frame(args);
  }
}

void sbx_Scene_event(sbx_SceneEventArgs* args) {
  if(args && args->scene && args->scene->event) {
    args->scene->event(args);
  }
}