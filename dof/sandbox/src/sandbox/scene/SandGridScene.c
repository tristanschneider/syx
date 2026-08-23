#include <sandbox/scene/SandGridScene.h>

#include <sandbox/Renderer.h>
#include <sandbox/scene/Scene.h>
#include <std/Allocator.h>
#include <std/Diagnostics.h>
#include <clm/transform25.h>
#include <clm/mat4.h>

#include <sandbox/ui/CameraUI.h>
#include <sandbox/ui/ModelUI.h>
#include <Nuklear/nuklear.h>
#include <sandbox/ui/nkExt.h>
#include <sandbox/SandGrid.h>
#include <sandbox/ui/TimeControlUI.h>

struct sbx_SelectedGrain {
  sbx_SandQueryResult query;
  clm_irect area;
};
typedef struct sbx_SelectedGrain sbx_SelectedGrain;

struct sbx_SandGridScene {
  sbx_Scene base;
  std_Allocator* alloc;
  sbx_Model quad;
  sbx_Renderable renderable;
  sbx_SandGridConfig config;
  sbx_SandGrid* sandGrid;
  sbx_Texture texture;
  sbx_SelectedGrain selected;
  sbx_TimeControlUI timeControl;
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

  const float s = 1.f;
  sbx_ModelVertex v[6] = { 0 };
  const clm_vec4 color = clm_vec4_splat(0);
  //Start at corner so model origin matches grid origin
  v[0] = (sbx_ModelVertex){ clm_vec3_ctor(0.f, s, 0.f), clm_vec2_ctor(0, 0), color };
  v[1] = (sbx_ModelVertex){ clm_vec3_ctor(s, s, 0.f), clm_vec2_ctor(1, 0), color };
  v[2] = (sbx_ModelVertex){ clm_vec3_ctor(s, 0.f, 0.f), clm_vec2_ctor(1, 1), color };

  v[3] = (sbx_ModelVertex){ clm_vec3_ctor(0.f, s, 0.f), clm_vec2_ctor(0, 0), color };
  v[4] = (sbx_ModelVertex){ clm_vec3_ctor(s, 0.f, 0.f), clm_vec2_ctor(1, 1), color };
  v[5] = (sbx_ModelVertex){ clm_vec3_ctor(0.f, 0.f, 0.f), clm_vec2_ctor(0, 1), color };

  sbx_Renderer_setModelVertices(args->renderer, self->quad, &(sbx_ModelVertices){
    .data = v,
    .count = (size_t)6
  });

  sbx_Renderer_setRenderableModel(args->renderer, self->renderable, self->quad);

  self->texture = sbx_Renderer_createTexture(args->renderer);
  sbx_Renderer_setRenderableTexture(args->renderer, self->renderable, self->texture);

  self->config = (sbx_SandGridConfig){
    .width = 100,
    .height = 100,
    .gravity = clm_vec2_ctor(0, -10.f)
  };
  self->sandGrid = sbx_createSandGrid(self->sandGrid, self->alloc, self->config);

  clm_transform25 rt = clm_transform25_identity();
  //Big enough to fill the screen, offset it centered in the screen, undoing the corner origin in the model above.
  rt.scale = clm_vec2_ctor(10, 10);
  rt.pos = clm_vec3_ctor(-s * 5.f, -s * 5.f, -0.1f);
  sbx_Renderer_setTransform(args->renderer, self->renderable, &rt);

  self->selected.area = clm_irect_limits();

  self->timeControl.timePerTick = 1.f/60.f;
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

clm_irect sbx_getMouseRect(sbx_SceneEventArgs* args) {
  sbx_SandGridScene* scene = (sbx_SandGridScene*)args->scene;
  clm_mat4 screenToWorld = sbx_Renderer_getScreenToWorld(args->renderer);
  clm_mat4 worldToScreen = clm_mat4_inverse(&screenToWorld);

  //Get the Z of the renderable in screen space
  const clm_transform25 rt = sbx_Renderer_getTransform(args->renderer, scene->renderable);
  const clm_vec3 objPos = rt.pos;
  clm_vec4 objZ = clm_vec4_ctor(0, 0, objPos.z, 1.f);
  objZ = clm_mat4_mul4(&worldToScreen, &objZ);
  objZ.z /= objZ.w;

  //Transform mouse position at Z depth of renderable in screen space back out to world
  //This means the mouse is in world space on the Z plane of the renderable
  const clm_vec4 mousePos = clm_vec4_ctor(args->mouseX, args->mouseY, objZ.z, 1);
  clm_vec4 mouseWorld = clm_mat4_mul4(&screenToWorld, &mousePos);
  mouseWorld.x /= mouseWorld.w;
  mouseWorld.y /= mouseWorld.w;
  mouseWorld.w = 1;

  //Transform the mouse from world to local space of the renderable
  clm_mat4 worldToRenderable = clm_transform25_toMatrix(&rt);
  worldToRenderable = clm_mat4_inverse(&worldToRenderable);
  mouseWorld = clm_mat4_mul4(&worldToRenderable, &mouseWorld);
  mouseWorld.y = 1.f - mouseWorld.y;

  const int ix = (int)(mouseWorld.x * (float)scene->config.width);
  const int iy = (int)(mouseWorld.y * (float)scene->config.height);
  return clm_irect_fromMinSize(ix, iy, 1, 1);
}

void sbx_tryInsertAtMouse(sbx_SceneEventArgs* args) {
  sbx_SandGridScene* scene = (sbx_SandGridScene*)args->scene;
  clm_irect mouseRect = sbx_getMouseRect(args);
  if(sbx_SandGrid_isValidRect(scene->sandGrid, &mouseRect)) {
    sbx_SandGridGrain grain = {
      .mass = 1,
      .shape = { SBX_GT_GRAIN },
      .color = clm_byte4_ctor(255, 0, 0, 0)
    };
    sbx_SandGrid_insert(&(sbx_SandGridInsertOps){
      .grid = scene->sandGrid,
      .grains = &grain,
      .grainCount = 1,
      .rect = &mouseRect,
      .mode = SBX_SGI_TRY
    });
  }
}

void sbx_trySelectAtMouse(sbx_SceneEventArgs* args) {
  sbx_SandGridScene* scene = (sbx_SandGridScene*)args->scene;
  clm_irect mouseRect = sbx_getMouseRect(args);
  //Will either be valid and populated with new data below or be invalid meaning the selected data is irrelevant.
  scene->selected.area = mouseRect;
  if(sbx_SandGrid_isValidRect(scene->sandGrid, &mouseRect)) {
    STD_ASSERT(clm_irect_area(&mouseRect) == 1);
    sbx_SandGrid_query(scene->sandGrid, &mouseRect, &scene->selected.query);
  }
}

void sbx_SandGridScene_event(sbx_SceneEventArgs* args) {
  switch(args->type) {
  case SBX_SCENEEVENT_MOUSE_DOWN:
    switch(args->button) {
      case SBX_BUTTON_LMB:
        sbx_tryInsertAtMouse(args);
        break;
      case SBX_BUTTON_RMB:
        sbx_trySelectAtMouse(args);
        break;
    }
    break;
  }
}

void sbx_SandGridScene_integrate(sbx_SandGridScene* scene) {
  clm_irect rect = clm_irect_limits();
  sbx_SandGrid_integrate(scene->sandGrid, &rect, scene->timeControl.timePerTick);
}

void sbx_SandGridScene_reset(sbx_SandGridScene* scene) {
  clm_irect rect = clm_irect_limits();
  rect = sbx_SandGrid_clipToGrid(scene->sandGrid, &rect);

  sbx_SandGrid_insert(&(sbx_SandGridInsertOps){
    .grid = scene->sandGrid,
    .mode = SBX_SGI_REPLACE,
    .rect = &rect,
  });
}

const char* sbx_grainTypeToString(sbx_GrainType type) {
  switch(type) {
    case SBX_GT_GRAIN: return "grain";
    case SBX_GT_STATIC: return "static";
    case SBX_GT_EMPTY: return "empty";
    default: return "invalid";
  }
}

void sbx_drawSelected(sbx_SceneFrameArgs* args) {
  sbx_SandGridScene* scene = (sbx_SandGridScene*)args->scene;
  nk_context* ctx = args->ctx;
  if(!sbx_SandGrid_isValidRect(scene->sandGrid, &scene->selected.area)) {
    return;
  }
  sbx_SandQueryResult* q = &scene->selected.query;

  //Refresh the query data in case it changed
  sbx_SandGrid_query(scene->sandGrid, &scene->selected.area, q);

  //Display query data
  nkx_label_format(ctx, "Type: %s", sbx_grainTypeToString(q->shape.type), 0);
  nkx_readonly_vec2(ctx, "Position", q->position);
  nkx_readonly_vec2(ctx, "Velocity", q->velocity);
  nkx_label_format(ctx, "Mass: %d", (int32_t)q->mass);
  nkx_label_format(ctx, "color %.8X", q->color);
}

void sbx_SandGridScene_frame(sbx_SceneFrameArgs* args) {
  sbx_SandGridScene* scene = (sbx_SandGridScene*)args->scene;
  nk_context* ctx = args->ctx;

  bool shouldDraw = false;
  if(nk_begin_titled(ctx, "sand", "Sand Grid", nk_rect(9, 9, 300, 400), nkx_titledWindow())) {
    nk_layout_row_dynamic(ctx, 0, 1);

    sbx_TimeControlUI_drawInline(ctx, &scene->timeControl, args->dt);

    sbx_drawSelected(args);
  }

  sbx_TimeControlUpdate update = sbx_TimeControlUI_tryUpdate(&scene->timeControl);
  if(update.tick) {
    sbx_SandGridScene_integrate(scene);
    shouldDraw = true;
  }
  if(update.reset) {
    sbx_SandGridScene_reset(scene);
    shouldDraw = true;
  }

  shouldDraw = true;
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
      .frame = &sbx_SandGridScene_frame,
      .event = &sbx_SandGridScene_event
    },
    .alloc = alloc,
  };
  return (sbx_Scene*)result;
}
