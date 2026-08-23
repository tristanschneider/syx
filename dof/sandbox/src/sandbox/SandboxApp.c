#include <sandbox/SandboxApp.h>

#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_log.h>
#include <sokol_glue.h>
#include <util/sokol_gl.h>
#include <std/Diagnostics.h>

#include <Nuklear/nuklear.h>
#include <util/sokol_nuklear.h>
#include <sandbox/Renderer.h>
#include <std/MallocAllocator.h>
#include <std/Allocator.h>

#include <sandbox/scene/Scene.h>
#include <sandbox/scene/NarrowphaseScene.h>
#include <sandbox/scene/SandGridScene.h>
#include <sandbox/ui/nkExt.h>

struct SandboxApp {
  sbx_Renderer* renderer;
  sbx_Scene* scene;
  std_Allocator allocator;
  bool sceneNeedsInit;
  bool isNKHovered;
};
typedef struct SandboxApp SandboxApp;

SandboxApp SANDBOX_APP;

SandboxApp* sbx_getApp() {
  return &SANDBOX_APP;
}

void sbx_setScene(SandboxApp* app, sbx_Scene* newScene) {
  if(app->scene) {
    sbx_Scene_dtor(&(sbx_SceneDtorArgs){
      .scene = app->scene,
      .renderer = app->renderer
    });
  }
  app->scene = newScene;
  app->sceneNeedsInit = true;
}

sbx_ButtonType mouseToButton(sapp_mousebutton mouse) {
  switch(mouse) {
    case SAPP_MOUSEBUTTON_LEFT: return SBX_BUTTON_LMB;
    case SAPP_MOUSEBUTTON_MIDDLE: return SBX_BUTTON_MMB;
    case SAPP_MOUSEBUTTON_RIGHT: return SBX_BUTTON_RMB;
  }
  return 0;
}

//0-1 to -1-1
float toNDC(float v) {
  return v * 2 - 1.f;
}

void fillMouse(sbx_SceneEventArgs* args, const sapp_event* event) {
  args->mouseX = event->mouse_x;
  args->mouseY = event->mouse_y;
  //Transform to NDC so using this on the view matrix will map directy to world space
  if(event->window_width) {
    args->mouseX /= (float)event->window_width;
  }
  if(event->window_height) {
    args->mouseY /= (float)event->window_height;
  }
  args->mouseX = toNDC(args->mouseX);
  args->mouseY = -toNDC(args->mouseY);
}

bool isNKHovered() {
  SandboxApp* app = sbx_getApp();
  return app && app->isNKHovered;
}

void onEvent(const sapp_event* event) {
  snk_handle_event(event);

  sbx_SceneEventArgs e;
  e.type = SBX_SCENEEVENT_INVALID;

  switch(event->type) {
    case SAPP_EVENTTYPE_RESIZED:
    case SAPP_EVENTTYPE_FOCUSED:
    case SAPP_EVENTTYPE_UNFOCUSED:
    //Mouse buttons are forwarded to the scene unless the button was on an nk window.
    //This prevents accidental input when interacting with a window.
    case SAPP_EVENTTYPE_MOUSE_DOWN:
      if(!isNKHovered()) {
        e.type = SBX_SCENEEVENT_MOUSE_DOWN;
        e.button = mouseToButton(event->mouse_button);
        fillMouse(&e, event);
      }
      break;
    case SAPP_EVENTTYPE_MOUSE_UP:
      if(!isNKHovered()) {
        e.type = SBX_SCENEEVENT_MOUSE_UP;
        e.button = mouseToButton(event->mouse_button);
        fillMouse(&e, event);
      }
      break;
    case SAPP_EVENTTYPE_KEY_DOWN:
    case SAPP_EVENTTYPE_KEY_UP:
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
    case SAPP_EVENTTYPE_MOUSE_MOVE:
      e.type = SBX_SCENEEVENT_MOUSE_MOVE;
      fillMouse(&e, event);
      break;
    case SAPP_EVENTTYPE_CHAR:
    case SAPP_EVENTTYPE_MOUSE_ENTER:
    case SAPP_EVENTTYPE_MOUSE_LEAVE:
    case SAPP_EVENTTYPE_TOUCHES_BEGAN:
    case SAPP_EVENTTYPE_TOUCHES_MOVED:
    case SAPP_EVENTTYPE_TOUCHES_ENDED:
    case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
    case SAPP_EVENTTYPE_ICONIFIED:
    case SAPP_EVENTTYPE_RESTORED:
    case SAPP_EVENTTYPE_SUSPENDED:
    case SAPP_EVENTTYPE_RESUMED:
    case SAPP_EVENTTYPE_QUIT_REQUESTED:
    case SAPP_EVENTTYPE_CLIPBOARD_PASTED:
    case SAPP_EVENTTYPE_FILES_DROPPED:
      break;
  }

  if(e.type != SBX_SCENEEVENT_INVALID) {
    SandboxApp* app = sbx_getApp();
    if(app) {
      e.scene = app->scene;
      e.renderer = app->renderer;
      sbx_Scene_event(&e);
    }
  }
}

void init(void) {
  SandboxApp* app = sbx_getApp();
  *app = (SandboxApp){
    .allocator = std_MallocAllocator_ctor()
  };
  sbx_setScene(app, sbx_SandGridScene_ctor(&app->allocator));

  //Initialize the graphics device
  sg_setup(&(sg_desc){
    .logger = {
      .func = slog_func,
    },
    .environment = sglue_environment(),
  });
  sgl_setup(&(sgl_desc_t){
    .color_format = SG_PIXELFORMAT_RGBA8,
    .depth_format = SG_PIXELFORMAT_DEPTH,
    .logger = {
      .func = slog_func,
    }
  });
  snk_setup(&(snk_desc_t){
      .enable_set_mouse_cursor = false,
      .dpi_scale = sapp_dpi_scale(),
      .logger.func = slog_func,
  });
}

void drawSceneSelector(SandboxApp* app, struct nk_context* ctx) {
  nk_flags flags = NK_HEADER_RIGHT | NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE;
  if (nk_begin_titled(ctx, "scene_selector", "Scene Selector", nk_rect(400, 10, 150, 100), flags)) {
    nk_layout_row_dynamic(ctx, 10, 1);
    if (nk_button_label(ctx, "Narrowphase")) {
      sbx_setScene(app, sbx_NarrowphaseScene_ctor(&app->allocator));
    }
    if (nk_button_label(ctx, "Sand Grid")) {
      sbx_setScene(app, sbx_SandGridScene_ctor(&app->allocator));
    }
  }
  nk_end(ctx);
}

void initRenderer(SandboxApp* app) {
  app->renderer = sbx_Renderer_ctor(&app->allocator);
}

void frame(void) {
  struct nk_context* ctx = snk_new_frame();
  SandboxApp* app = sbx_getApp();
  app->isNKHovered = nk_window_is_any_hovered(ctx);

  if(!app->renderer) {
    initRenderer(app);
  }

  nk_style_hide_cursor(ctx);

  drawSceneSelector(app, ctx);

  if(app->scene) {
    if(app->sceneNeedsInit) {
      app->sceneNeedsInit = false;
      sbx_Scene_init(&(sbx_SceneInitArgs){
        .scene = app->scene,
        .renderer = app->renderer,
      });
    }

    sbx_Scene_frame(&(sbx_SceneFrameArgs){
      .scene = app->scene,
      .renderer = app->renderer,
      .ctx = ctx,
      .dt = (float)sapp_frame_duration()
    });
  }

  nk_end(ctx);

  sbx_Renderer_render(app->renderer);
}

void cleanup(void) {
  SandboxApp* app = sbx_getApp();
  sbx_setScene(app, NULL);

  //sfons_destroy(state.fontContext);
  //state.fontContext = nullptr;
  snk_shutdown();
  sgl_shutdown();
  sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
  STD_UNUSED(argc, argv);
  return (sapp_desc) {
    .init_cb = init,
    .frame_cb = frame,
    .cleanup_cb = cleanup,
    .event_cb = onEvent,
    .width = 640,
    .height = 480,
    .swap_interval = 1,
    .window_title = "Sandbox",
    .icon = (sapp_icon_desc) {
      .sokol_default = true,
    },
    .logger = (sapp_logger) {
      .func = slog_func
    },
    .win32_console_create = true
  };
}
