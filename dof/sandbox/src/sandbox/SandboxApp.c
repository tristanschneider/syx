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
#include <sandbox/ui/CameraUI.h>

struct SandboxApp {
  sbx_Renderer* renderer;
  sbx_Model quad;
  sbx_Renderable renderable;
  std_Allocator allocator;
};
typedef struct SandboxApp SandboxApp;

SandboxApp SANDBOX_APP;

SandboxApp* sbx_getApp() {
  return &SANDBOX_APP;
}

void onEvent(const sapp_event* event) {
  snk_handle_event(event);

  switch(event->type) {
    case SAPP_EVENTTYPE_RESIZED:
    case SAPP_EVENTTYPE_FOCUSED:
    case SAPP_EVENTTYPE_UNFOCUSED:
    case SAPP_EVENTTYPE_MOUSE_DOWN:
    case SAPP_EVENTTYPE_MOUSE_UP:
    case SAPP_EVENTTYPE_KEY_DOWN:
    case SAPP_EVENTTYPE_KEY_UP:
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
    case SAPP_EVENTTYPE_MOUSE_MOVE:
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
}

void init(void) {
  *sbx_getApp() = (SandboxApp){
    .allocator = std_MallocAllocator_ctor()
  };

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

void drawUI(struct nk_context* ctx) {
  nk_style_hide_cursor(ctx);

  nk_flags flags = NK_HEADER_RIGHT | NK_WINDOW_BORDER | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE;
  if (nk_begin(ctx, "test", nk_rect(10, 10, 400, 400), flags)) {
    nk_layout_row_dynamic(ctx, 10, 1);
    nk_label(ctx, "label A", NK_TEXT_LEFT);

    if (nk_tree_push(ctx, NK_TREE_NODE, "tree", NK_MINIMIZED)) {
      nk_label(ctx, "label B", NK_TEXT_LEFT);
      if(nk_button_label(ctx, "button")) {
        LOGI("clicked");
      }

      nk_tree_pop(ctx);
    }
  }
  nk_end(ctx);
}

void initRenderer(SandboxApp* app) {
  app->renderer = sbx_Renderer_ctor(&app->allocator);
  app->quad = sbx_Renderer_createModel(app->renderer);
  app->renderable = sbx_Renderer_createRenderable(app->renderer);

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
  sbx_Renderer_setModelVertices(app->renderer, app->quad, &(sbx_ModelVertices){
    .data = v,
    .count = (size_t)6
  });

  sbx_Renderer_setRenderableModel(app->renderer, app->renderable, app->quad);
}

void frame(void) {
  struct nk_context *ctx = snk_new_frame();
  SandboxApp* app = sbx_getApp();

  if(!app->renderer) {
    initRenderer(app);
  }

  nk_style_hide_cursor(ctx);
  sbx_CameraUI_draw(ctx, app->renderer);

  sbx_Renderer_render(app->renderer);
}

void cleanup(void) {
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
