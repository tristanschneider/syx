#include <sandbox/scene/SandGridScene.h>

#include <sandbox/Renderer.h>
#include <sandbox/scene/Scene.h>
#include <std/Allocator.h>
#include <std/Diagnostics.h>
#include <clm/transform25.h>

#include <sandbox/ui/CameraUI.h>
#include <sandbox/ui/ModelUI.h>
#include <Nuklear/nuklear.h>
#include <sandbox/ui/nkExt.h>
#include <sandbox/SandGrid.h>

struct sbx_SandGridScene {
  sbx_Scene base;
  std_Allocator* alloc;
  sbx_Model quad;
  sbx_Renderable renderable;
  sbx_SandGridConfig config;
  sbx_SandGrid* sandGrid;
  sbx_Texture texture;
};
typedef struct sbx_SandGridScene sbx_SandGridScene;

sbx_SandGrid* sbx_createSandGrid(sbx_SandGrid* sandGrid, std_Allocator* alloc, sbx_SandGridConfig config) {
  if(sandGrid) {
    std_Allocator_dealloc(alloc, sandGrid);
  }
  return sbx_SandGrid_ctor(alloc, config);
}

void sbx_SandGridScene_init(sbx_SceneInitArgs* args) {
  sbx_SandGridScene* self = (sbx_SandGridScene*)args->scene;
  self->quad = sbx_Renderer_createModel(args->renderer);
  self->renderable = sbx_Renderer_createRenderable(args->renderer);

  const float s = 0.5f;
  sbx_ModelVertex v[6] = { 0 };
  const clm_vec4 color = clm_vec4_splat(1);
  v[0] = (sbx_ModelVertex){ clm_vec3_ctor(-s, s, 0.f), clm_vec2_ctor(0, 0), color };
  v[1] = (sbx_ModelVertex){ clm_vec3_ctor(s, s, 0.f), clm_vec2_ctor(1, 0), color };
  v[2] = (sbx_ModelVertex){ clm_vec3_ctor(s, -s, 0.f), clm_vec2_ctor(1, 1), color };

  v[3] = (sbx_ModelVertex){ clm_vec3_ctor(-s, s, 0.f), clm_vec2_ctor(0, 0), color };
  v[4] = (sbx_ModelVertex){ clm_vec3_ctor(s, -s, 0.f), clm_vec2_ctor(1, 1), color };
  v[5] = (sbx_ModelVertex){ clm_vec3_ctor(-s, -s, 0.f), clm_vec2_ctor(0, 1), color };

  sbx_Renderer_setModelVertices(args->renderer, self->quad, &(sbx_ModelVertices){
    .data = v,
    .count = (size_t)6
  });

  sbx_Renderer_setRenderableModel(args->renderer, self->renderable, self->quad);

  self->texture = sbx_Renderer_createTexture(args->renderer);
  sbx_Renderer_setRenderableTexture(args->renderer, self->renderable, self->texture);

  self->config = (sbx_SandGridConfig){
    .width = 10,
    .height = 10,
    .gravity = clm_vec2_ctor(0, -1.f)
  };
  self->sandGrid = sbx_createSandGrid(self->sandGrid, self->alloc, self->config);
}

void sbx_SandGridScene_dtor(sbx_SceneDtorArgs* args) {
  sbx_SandGridScene* self = (sbx_SandGridScene*)args->scene;

  if(self->quad.data) {
    sbx_Renderer_destroyModel(args->renderer, self->quad);
  }
  if(self->renderable.data) {
    sbx_Renderer_destroyRenderable(args->renderer, self->renderable);
  }

  std_Allocator_dealloc(self->alloc, self);
}

void sbx_SandGridScene_frame(sbx_SceneFrameArgs* args) {
  sbx_SandGridScene* scene = (sbx_SandGridScene*)args->scene;
  nk_context* ctx = args->ctx;

  nk_flags flags = NK_HEADER_RIGHT | NK_WINDOW_BORDER | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE;
  bool shouldDraw = false;
  if(nk_begin_titled(ctx, "sand", "Sand Grid", nk_rect(9, 9, 300, 400), flags)) {
    nk_layout_row_dynamic(ctx, 0, 1);
    if(nk_button_label(ctx, "Step")) {
      shouldDraw = true;

    }
    if(nk_button_label(ctx, "Reset")) {
      shouldDraw = true;
    }
  }

  if(shouldDraw) {
    const clm_byte4* data = sbx_SandGrid_getTexture(scene->sandGrid);
    sbx_Renderer_setTexture(args->renderer, scene->texture, &(sbx_TextureContents){
      .data = data,
      .width = (uint32_t)scene->config.width,
      .height = (uint32_t)scene->config.height
    });
  }
}

sbx_Scene* sbx_SandGridScene_ctor(std_Allocator* alloc) {
  sbx_SandGridScene* result = std_Allocator_alloc(alloc, sizeof(sbx_SandGridScene));
  *result = (sbx_SandGridScene){
    .base = (sbx_Scene){
      .dtor = &sbx_SandGridScene_dtor,
      .init = &sbx_SandGridScene_init,
      .frame = &sbx_SandGridScene_frame
    },
    .alloc = alloc,
  };
  return (sbx_Scene*)result;
}
