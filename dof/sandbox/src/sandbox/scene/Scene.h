#pragma once

#include <float.h>

struct sbx_Renderer;
struct nk_context;
struct sbx_Scene;

typedef struct sbx_Renderer sbx_Renderer;
typedef struct sbx_Scene sbx_Scene;
typedef struct nk_context nk_context;

struct sbx_SceneDtorArgs {
  sbx_Scene* scene;
  sbx_Renderer* renderer;
};
typedef struct sbx_SceneDtorArgs sbx_SceneDtorArgs;

struct sbx_SceneInitArgs {
  sbx_Scene* scene;
  sbx_Renderer* renderer;
};
typedef struct sbx_SceneInitArgs sbx_SceneInitArgs;

struct sbx_SceneFrameArgs {
  sbx_Scene* scene;
  sbx_Renderer* renderer;
  nk_context* ctx;
  float dt;
};
typedef struct sbx_SceneFrameArgs sbx_SceneFrameArgs;

enum sbx_SceneEventType {
  SBX_SCENEEVENT_INVALID,
  SBX_SCENEEVENT_MOUSE_DOWN,
  SBX_SCENEEVENT_MOUSE_UP,
  SBX_SCENEEVENT_MOUSE_MOVE,
};
typedef enum sbx_SceneEventType sbx_SceneEventType;

enum sbx_ButtonType {
  // Left, right, middle mouse
  SBX_BUTTON_LMB = 500,
  SBX_BUTTON_RMB = 501,
  SBX_BUTTON_MMB = 502,
};
typedef enum sbx_ButtonType sbx_ButtonType;

struct sbx_SceneEventArgs {
  sbx_Scene* scene;
  sbx_Renderer* renderer;
  sbx_SceneEventType type;
  //If it's mouse up or down
  sbx_ButtonType button;
  float mouseX;
  float mouseY;
};
typedef struct sbx_SceneEventArgs sbx_SceneEventArgs;

struct sbx_Scene {
  void(*dtor)(sbx_SceneDtorArgs*);
  void(*init)(sbx_SceneInitArgs*);
  void(*frame)(sbx_SceneFrameArgs*);
  void(*event)(sbx_SceneEventArgs*);
};

void sbx_Scene_dtor(sbx_SceneDtorArgs* scene);
void sbx_Scene_init(sbx_SceneInitArgs* args);
void sbx_Scene_frame(sbx_SceneFrameArgs* args);
void sbx_Scene_event(sbx_SceneEventArgs* args);