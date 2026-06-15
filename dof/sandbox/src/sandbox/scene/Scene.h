#pragma once

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
};
typedef struct sbx_SceneFrameArgs sbx_SceneFrameArgs;

struct sbx_Scene {
  void(*dtor)(sbx_SceneDtorArgs*);
  void(*init)(sbx_SceneInitArgs*);
  void(*frame)(sbx_SceneFrameArgs*);
};

void sbx_Scene_dtor(sbx_SceneDtorArgs* scene);
void sbx_Scene_init(sbx_SceneInitArgs* args);
void sbx_Scene_frame(sbx_SceneFrameArgs* args);
