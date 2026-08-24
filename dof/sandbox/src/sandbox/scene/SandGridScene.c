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
#include <sandbox/SandGridImpulse.h>

struct sbx_SelectedGrain {
  sbx_SandQueryResult query;
  clm_irect area;
};
typedef struct sbx_SelectedGrain sbx_SelectedGrain;

enum sbx_InteractType {
  SBX_INTERACTTYPE_INSERT,
  SBX_INTERACTTYPE_REPEL,
  SBX_INTERACTTYPE_SELECT,
  SBX_INTERACTTYPE_COUNT
};
typedef enum sbx_InteractType sbx_InteractType;

enum sbx_MouseMode {
  SBX_MOUSEMODE_CLICK,
  SBX_MOUSEMODE_CLICKANDHOLD,
  SBX_MOUSEMODE_TOGGLE,
};
typedef enum sbx_MouseMode sbx_MouseMode;

struct sbx_Interaction {
  bool isOn;
  bool isAlwaysOn;
  const char* name;
};
typedef struct sbx_Interaction sbx_Interaction;

struct sbx_InteractMode {
  sbx_MouseMode mouse;
  sbx_InteractType lmbInteract;
  sbx_InteractType rmbInteract;
  sbx_Interaction types[SBX_INTERACTTYPE_COUNT];
};
typedef struct sbx_InteractMode sbx_InteractMode;

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
  sbx_InteractMode interact;
  clm_vec2 lastMouse;
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

  self->interact.mouse = SBX_MOUSEMODE_CLICKANDHOLD;
  self->interact.lmbInteract = SBX_INTERACTTYPE_INSERT;
  self->interact.rmbInteract = SBX_INTERACTTYPE_SELECT;
  self->interact.types[SBX_INTERACTTYPE_SELECT] = (sbx_Interaction){
    .isOn = true,
    .isAlwaysOn = true
  };
  self->interact.types[SBX_INTERACTTYPE_INSERT].name = "insert";
  self->interact.types[SBX_INTERACTTYPE_REPEL].name = "repel";
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

clm_irect sbx_getMouseRect(sbx_SandGridScene* scene, sbx_Renderer* renderer) {
  clm_mat4 screenToWorld = sbx_Renderer_getScreenToWorld(renderer);
  clm_mat4 worldToScreen = clm_mat4_inverse(&screenToWorld);

  //Get the Z of the renderable in screen space
  const clm_transform25 rt = sbx_Renderer_getTransform(renderer, scene->renderable);
  const clm_vec3 objPos = rt.pos;
  clm_vec4 objZ = clm_vec4_ctor(0, 0, objPos.z, 1.f);
  objZ = clm_mat4_mul4(&worldToScreen, &objZ);
  objZ.z /= objZ.w;

  //Transform mouse position at Z depth of renderable in screen space back out to world
  //This means the mouse is in world space on the Z plane of the renderable
  const clm_vec4 mousePos = clm_vec4_ctor(scene->lastMouse.x, scene->lastMouse.y, objZ.z, 1);
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

void sbx_tryInsertAtMouse(sbx_SandGridScene* scene, clm_irect mouseRect) {
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

void sbx_repelAtMouse(sbx_SandGridScene* scene, clm_irect mouseRect) {
  clm_irect repelRect = clm_irect_grow(&mouseRect, 10, 10);
  sbx_SandGridImpulse_Radial impulse = {
    .center = clm_vec2_ctor((float)mouseRect.minX, (float)mouseRect.minY),
    .radius = 10,
    .scalar = 0.1f
  };
  sbx_SandGrid_applyImpulse(scene->sandGrid, &repelRect, sbx_SandGridImpulse_createRadial(&impulse));
}

void sbx_trySelectAtMouse(sbx_SandGridScene* scene, clm_irect mouseRect, bool startNewInteraction) {
  //Will either be valid and populated with new data below or be invalid meaning the selected data is irrelevant.
  if(startNewInteraction) {
    scene->selected.area = mouseRect;
  }

  if(sbx_SandGrid_isValidRect(scene->sandGrid, &scene->selected.area)) {
    STD_ASSERT(clm_irect_area(&mouseRect) == 1);
    sbx_SandGrid_query(scene->sandGrid, &scene->selected.area, &scene->selected.query);
  }
}

void sbx_tryInteractAtMouse(sbx_SandGridScene* scene, sbx_Renderer* renderer, sbx_InteractType type, bool startNewInteraction) {
  //If this shouldn't start a new interaction and one isn't active, nothing to do
  sbx_Interaction* interaction = &scene->interact.types[type];
  if(!startNewInteraction && !interaction->isOn) {
    return;
  }
  clm_irect mouseRect = sbx_getMouseRect(scene, renderer);

  switch(type) {
    case SBX_INTERACTTYPE_INSERT:
      sbx_tryInsertAtMouse(scene, mouseRect);
      break;
    case SBX_INTERACTTYPE_REPEL:
      sbx_repelAtMouse(scene, mouseRect);
      break;
    case SBX_INTERACTTYPE_SELECT:
      sbx_trySelectAtMouse(scene, mouseRect, startNewInteraction);
      break;
  }
}

sbx_Interaction* sbx_getInteraction(sbx_InteractType type, sbx_InteractMode* mode) {
  return (uint32_t)type < SBX_INTERACTTYPE_COUNT ? &mode->types[type] : NULL;
}

sbx_InteractType sbx_getInteractionType(const sbx_Interaction* type, const sbx_InteractMode* mode) {
  return (sbx_InteractType)(type - &mode->types[0]);
}

sbx_Interaction* sbx_getInteractFromButton(sbx_InteractMode* mode, sbx_ButtonType type) {
  switch(type) {
    case SBX_BUTTON_LMB: return sbx_getInteraction(mode->lmbInteract, mode);
    case SBX_BUTTON_RMB: return sbx_getInteraction(mode->rmbInteract, mode);
    default: return NULL;
  }
}

void sbx_updateInteractModeEnabled(sbx_Interaction* interaction, bool down, sbx_MouseMode mouseMode) {
  if(!interaction) {
    return;
  }
  switch(mouseMode) {
    case SBX_MOUSEMODE_CLICK:
      break;
    case SBX_MOUSEMODE_CLICKANDHOLD:
      interaction->isOn = down || interaction->isAlwaysOn;
      break;
    case SBX_MOUSEMODE_TOGGLE:
      if(down) {
        interaction->isOn = !interaction->isOn || interaction->isAlwaysOn;
      }
      break;
  }
}

void sbx_SandGridScene_event(sbx_SceneEventArgs* args) {
  sbx_SandGridScene* scene = (sbx_SandGridScene*)args->scene;
  scene->lastMouse = clm_vec2_ctor(args->mouseX, args->mouseY);

  sbx_Interaction* interactDown = NULL;
  sbx_Interaction* interactUp = NULL;
  switch(args->type) {
    case SBX_SCENEEVENT_MOUSE_DOWN:
      interactDown = sbx_getInteractFromButton(&scene->interact, args->button);
      break;
    case SBX_SCENEEVENT_MOUSE_UP:
      interactUp = sbx_getInteractFromButton(&scene->interact, args->button);
      break;
  }

  sbx_updateInteractModeEnabled(interactDown, true, scene->interact.mouse);
  sbx_updateInteractModeEnabled(interactUp, false, scene->interact.mouse);
  if(interactDown) {
    sbx_tryInteractAtMouse(scene, args->renderer, sbx_getInteractionType(interactDown, &scene->interact), true);
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

  for(int i = 0; i < SBX_INTERACTTYPE_COUNT; ++i) {
    sbx_tryInteractAtMouse(scene, args->renderer, (sbx_InteractType)i, false);
  }

  bool shouldDraw = false;
  if(nk_begin_titled(ctx, "sand", "Sand Grid", nk_rect(9, 9, 300, 400), nkx_titledWindow())) {
    nk_layout_row_dynamic(ctx, 0, 1);

    sbx_TimeControlUI_drawInline(ctx, &scene->timeControl, args->dt);

    nk_bool nkTrue = true;
    nk_bool nkFalse = false;
    for(int i = 0; i < SBX_INTERACTTYPE_COUNT; ++i) {
      sbx_Interaction* interaction = &scene->interact.types[i];
      if(interaction->name) {
        if(nk_radio_label(ctx, interaction->name, i == scene->interact.lmbInteract ? &nkTrue : &nkFalse)) {
          scene->interact.lmbInteract = i;
        }
      }
    }

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
