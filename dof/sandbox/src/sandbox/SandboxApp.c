#include <sandbox/SandboxApp.h>

#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_log.h>
#include <sokol_glue.h>
//#include "fontstash.h"
//#include <util/sokol_fontstash.h>
#include <util/sokol_gl.h>
#include <std/Diagnostics.h>

void onEvent(const sapp_event* event) {
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
  //sfons_desc_t fd = {
  //  .width = 1024,
  //  .height = 1024,
  //};
  //state.fontContext = sfons_create(&fd);
}

void frame(void) {
}

void cleanup(void) {
  //sfons_destroy(state.fontContext);
  //state.fontContext = nullptr;
  sgl_shutdown();
  sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
  STD_UNUSED(argc, argv);
  //for(int i = 0; i < argc; ++i) {
  //  state.args.emplace_back(argv[i]);
  //}
  return (sapp_desc) {
    .init_cb = init,
    .frame_cb = frame,
    .cleanup_cb = cleanup,
    .event_cb = onEvent,
    .width = 640,
    .height = 480,
    //Match monitor refresh rate one to one
    //If I knew how to ask what the refresh rate was I would use a ratio to put it at 60fps
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
