#include <sandbox/scene/NarrowphaseScene.h>

#include <sandbox/Renderer.h>
#include <sandbox/scene/Scene.h>
#include <std/Allocator.h>
#include <std/Diagnostics.h>
#include <clm/transform25.h>

#include <sandbox/ui/CameraUI.h>
#include <sandbox/ui/ModelUI.h>
#include <Nuklear/nuklear.h>
#include <sandbox/ui/nkExt.h>

struct sbx_NarrowphaseScene {
  sbx_Scene base;
  std_Allocator* alloc;
  sbx_Model quad;
  sbx_Renderable renderable;
};
typedef struct sbx_NarrowphaseScene sbx_NarrowphaseScene;

void sbx_NarrowphaseScene_init(sbx_SceneInitArgs* args) {
  sbx_NarrowphaseScene* self = (sbx_NarrowphaseScene*)args->scene;
  self->quad = sbx_Renderer_createModel(args->renderer);
  self->renderable = sbx_Renderer_createRenderable(args->renderer);

  const float s = 0.5f;
  sbx_ModelVertex v[6] = { 0 };
  v[0].pos = clm_vec3_ctor(-s, s, 0.f);
  v[1].pos = clm_vec3_ctor(s, s, 0.f);
  v[2].pos = clm_vec3_ctor(s, -s, 0.f);

  v[3].pos = clm_vec3_ctor(-s, s, 0.f);
  v[4].pos = clm_vec3_ctor(s, -s, 0.f);
  v[5].pos = clm_vec3_ctor(-s, -s, 0.f);
  for(int i = 0; i < 6; ++i) {
    v[i].color = clm_vec4_splat(1.f);
  }
  v[0].color = clm_vec4_ctor(1, 0, 0, 1);
  sbx_Renderer_setModelVertices(args->renderer, self->quad, &(sbx_ModelVertices){
    .data = v,
    .count = (size_t)6
  });

  sbx_Renderer_setRenderableModel(args->renderer, self->renderable, self->quad);
}

void sbx_NarrowphaseScene_dtor(sbx_SceneDtorArgs* args) {
  sbx_NarrowphaseScene* self = (sbx_NarrowphaseScene*)args->scene;

  if(self->quad.data) {
    sbx_Renderer_destroyModel(args->renderer, self->quad);
  }
  if(self->renderable.data) {
    sbx_Renderer_destroyRenderable(args->renderer, self->renderable);
  }

  std_Allocator_dealloc(self->alloc, self);
}

void sbx_NarrowphaseScene_frame(sbx_SceneFrameArgs* args) {
  sbx_NarrowphaseScene* scene = (sbx_NarrowphaseScene*)args->scene;
  nk_context* ctx = args->ctx;
  sbx_CameraUI_draw(ctx, args->renderer);

  nk_flags flags = NK_HEADER_RIGHT | NK_WINDOW_BORDER | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE;
  if(nk_begin_titled(ctx, "obj", "Object", nk_rect(9, 9, 300, 200), flags)) {
    nk_layout_row_dynamic(ctx, 0, 1);
    clm_transform25 transform = sbx_Renderer_getTransform(args->renderer, scene->renderable);
    if(nkx_property_transform25(ctx, "Transform", &transform)) {
      sbx_Renderer_setTransform(args->renderer, scene->renderable, &transform);
    }
    sbx_ModelVertices* newVerts = sbx_ModelUI_draw(ctx, sbx_Renderer_getModelVertices(args->renderer, scene->quad), scene->alloc);
    if(newVerts) {
      sbx_Renderer_setModelVertices(args->renderer, scene->quad, newVerts);
      std_Allocator_dealloc(scene->alloc, newVerts);
    }
  }
}

sbx_Scene* sbx_NarrowphaseScene_ctor(std_Allocator* alloc) {
  sbx_NarrowphaseScene* result = std_Allocator_alloc(alloc, sizeof(sbx_NarrowphaseScene));
  *result = (sbx_NarrowphaseScene){
    .base = (sbx_Scene){
      .dtor = &sbx_NarrowphaseScene_dtor,
      .init = &sbx_NarrowphaseScene_init,
      .frame = &sbx_NarrowphaseScene_frame
    },
    .alloc = alloc
  };
  return (sbx_Scene*)result;
}
