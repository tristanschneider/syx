
#include "Precompile.h"

//For console
#include <io.h>
#include <fcntl.h>

#include <Windows.h>

#include "GameBuilder.h"
#include "GameDatabase.h"
#include "GameScheduler.h"
#include "Simulation.h"
#include "ThreadLocals.h"
#include "Renderer.h"
#include "GraphViz.h"
#include "GameInput.h"

#include "glm/gtx/norm.hpp"
#include "Profile.h"

#include "AppBuilder.h"
#include "Simulation.h"

#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_log.h"
#include "sokol_glue.h"
#include "fontstash.h"
#include "util/sokol_fontstash.h"
#include "util/sokol_gl.h"

#include "Game.h"
#include "IGame.h"

#ifdef IMGUI_ENABLED
#include "ImguiModule.h"
#include "util/sokol_imgui.h"
#endif

//TODO: add capabilities to state machine so this keyboard passthrough isn't necessary.
//The passthrough is only used for imgui debugging, gameplay should use the state machine
using PlayerInputQueryResult = QueryResult<GameInput::PlayerKeyboardInputRow>;
using InputMachineQueryResult = QueryResult<GameInput::StateMachineRow>;

struct InputArgs {
  PlayerInputQueryResult playerInput;
  InputMachineQueryResult machineInput;
};

struct AppDatabase {
  IDatabase* combined{};
  Row<WindowData>* window{};
  InputArgs input;
};

namespace {
  AppDatabase* APP = nullptr;
}

struct AppState {
  sg_pipeline pip;
  sg_bindings bind;
  sg_pass_action pass_action;
  AppDatabase app;
  std::unique_ptr<IGame> game;
  std::vector<std::string> args;
  std::chrono::steady_clock::time_point lastDraw;
  FONScontext* fontContext{};
};
AppState state;

WindowData* tryGetWindow() {
  return APP && APP->window && APP->window->size() ? &APP->window->at(0) : nullptr;
}

void setWindowSize(int width, int height) {
  if(WindowData* window = tryGetWindow()) {
    window->hasChanged = width != window->mWidth || height != window->mHeight;
    window->mWidth = width;
    window->mHeight = height;
  }
}

void onFocusChanged(bool isFocused) {
  if(WindowData* window = tryGetWindow()) {
    window->mFocused = isFocused;
  }
}

void onKey(Input::PlatformInputID key, InputArgs& args, GameInput::KeyState s) {
  const bool isDown = s == GameInput::KeyState::Triggered;

  for(size_t t = 0; t < args.machineInput.size(); ++t) {
    auto [machines] = args.machineInput.get(t);
    for(Input::StateMachine& machine : *machines) {
      //Would be most efficient to put the condition outside the loop but presumably there's only one machine anyway
      if(isDown) {
        machine.traverse(machine.getMapper().onKeyDown(static_cast<Input::PlatformInputID>(key)));
      }
      else {
        machine.traverse(machine.getMapper().onKeyUp(static_cast<Input::PlatformInputID>(key)));
      }
    }
  }

  for(size_t t = 0; t < args.playerInput.size(); ++t) {
    auto&& [keyboards] = args.playerInput.get(t);
    for(size_t i = 0; i < keyboards->size(); ++i) {
      GameInput::PlayerKeyboardInput& keyboard = keyboards->at(i);
      keyboard.mRawKeys.push_back(std::make_pair(s, (int)key));
    }
  }
}

void createKeyboardMappings(Input::InputMapper& mapper) {
  auto cast = [](auto i) { return static_cast<Input::PlatformInputID>(i); };
  mapper.addKeyAs2DRelativeMapping(cast(SAPP_KEYCODE_W), GameInput::Keys::MOVE_2D, { 0, 1 });
  mapper.addKeyAs2DRelativeMapping(cast(SAPP_KEYCODE_A), GameInput::Keys::MOVE_2D, { -1, 0 });
  mapper.addKeyAs2DRelativeMapping(cast(SAPP_KEYCODE_S), GameInput::Keys::MOVE_2D, { 0, -1 });
  mapper.addKeyAs2DRelativeMapping(cast(SAPP_KEYCODE_D), GameInput::Keys::MOVE_2D, { 1, 0 });
  mapper.addKeyAs1DRelativeMapping(cast(SAPP_KEYCODE_PAGE_UP), GameInput::Keys::DEBUG_ZOOM_1D, -1.0f);
  mapper.addKeyAs1DRelativeMapping(cast(SAPP_KEYCODE_PAGE_DOWN), GameInput::Keys::DEBUG_ZOOM_1D, 1.0f);
  mapper.addKeyMapping(cast(SAPP_KEYCODE_SPACE), GameInput::Keys::ACTION_1);
  mapper.addKeyMapping(cast(SAPP_KEYCODE_LEFT_SHIFT), GameInput::Keys::ACTION_2);
  mapper.addKeyMapping(cast(SAPP_KEYCODE_ENTER), GameInput::Keys::JUMP);
  mapper.addKeyAs1DRelativeMapping(cast(SAPP_KEYCODE_E), GameInput::Keys::CHANGE_DENSITY_1D, 1.0f);
  mapper.addKeyAs1DRelativeMapping(cast(SAPP_KEYCODE_Q), GameInput::Keys::CHANGE_DENSITY_1D, -1.0f);
}

struct SokolInputModule : IAppModule {
  void init(IAppBuilder& builder) final {
    if(builder.getEnv().type == AppEnvType::InitThreadLocal) {
      return;
    }

    auto task = builder.createTask();
    Input::InputMapper* mapper = task.query<GameInput::GlobalMappingsRow>().tryGetSingletonElement();

    task.setCallback([mapper](AppTaskArgs&) {
      createKeyboardMappings(*mapper);
    });

    builder.submitTask(std::move(task.setName("build game mappings")));
  }
};

//Must be registered before simulation init because that tries to load from a config that needs the launch path
struct LaunchArgsModule : IAppModule {
  void init(IAppBuilder& builder) final {
    auto task = builder.createTask();
    FileSystem* fs = task.query<SharedRow<FileSystem>>().tryGetSingletonElement();

    task.setCallback([fs](AppTaskArgs&) {
      if(!fs) {
        return;
      }
      //First argument is the executable location, second and beyond are arguments passed to the executable
      fs->mRoot = state.args.size() >= 2 ? state.args[1] : "data/";
    });

    builder.submitTask(std::move(task.setName("fs")));
  }
};

void enqueueMouseWheel(InputArgs& db, float dx, float dy) {
  db;dx;dy;
}

void onMouseWheel(float dx, float dy) {
  if(APP) {
    enqueueMouseWheel(APP->input, dx, dy);
  }
}

GameInput::PlayerKeyboardInput* tryGetInput() {
  return APP ? APP->input.playerInput.tryGetSingletonElement<0>() : nullptr;
}

void onMouseMove(const sapp_event& event) {
  if(GameInput::PlayerKeyboardInput* input = tryGetInput()) {
    input->mRawMouseDeltaPixels = { event.mouse_dx, event.mouse_dy };
    //Valid unless mouse is locked
    input->mRawMousePixels = { event.mouse_x, event.mouse_y };
  }
}

void onChar(uint32_t utf32) {
  //isPrint to determine if this is a character we care about, which is undefined if value doesn't fit in unsigned char
  if(GameInput::PlayerKeyboardInput* input = tryGetInput(); input && utf32 <= 255 && std::isprint(static_cast<int>(utf32))) {
    input->mRawText.push_back(static_cast<char>(utf32));
  }
}

constexpr Input::PlatformInputID toKeycode(sapp_mousebutton key) {
  return static_cast<Input::PlatformInputID>(key) + 500;
}

constexpr Input::PlatformInputID toKeycode(sapp_keycode key) {
  return static_cast<Input::PlatformInputID>(key);
}

void doKey(const sapp_event& event, GameInput::KeyState s) {
  //Ignore key repeat events (down events fired while the key is down)
  if(!event.key_repeat && APP) {
    onKey(event.key_code, APP->input, s);
  }
}

void doMouseKey(sapp_mousebutton key, GameInput::KeyState s) {
  if(APP) {
    onKey(toKeycode(key), APP->input, s);
  }
}

void onEvent(const sapp_event* event) {
#ifdef IMGUI_ENABLED
  simgui_handle_event(event);
#endif

  switch(event->type) {
    case SAPP_EVENTTYPE_RESIZED:
      setWindowSize(event->framebuffer_width, event->framebuffer_height);
      break;
    case SAPP_EVENTTYPE_FOCUSED:
      onFocusChanged(true);
      break;
    case SAPP_EVENTTYPE_UNFOCUSED:
      onFocusChanged(false);
      break;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
      doMouseKey(event->mouse_button, GameInput::KeyState::Triggered);
      break;
    case SAPP_EVENTTYPE_MOUSE_UP:
      doMouseKey(event->mouse_button, GameInput::KeyState::Released);
      break;
    case SAPP_EVENTTYPE_KEY_DOWN:
      doKey(*event, GameInput::KeyState::Triggered);
      break;
    case SAPP_EVENTTYPE_KEY_UP:
      doKey(*event, GameInput::KeyState::Released);
      break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
      onMouseWheel(event->scroll_x, event->scroll_y);
      break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
      onMouseMove(*event);
      break;
    case SAPP_EVENTTYPE_CHAR:
      onChar(event->char_code);
      break;
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

std::unique_ptr<IGame> createGame(const RendererContext& renderCtx) {
  Game::GameArgs gameArgs = GameDefaults::createDefaultGameArgs();
  gameArgs.rendering = Renderer::createModule(renderCtx);
  gameArgs.modules.insert(gameArgs.modules.begin(), std::make_unique<LaunchArgsModule>());
  gameArgs.modules.push_back(std::make_unique<SokolInputModule>());
#ifdef IMGUI_ENABLED
  gameArgs.modules.push_back(ImguiModule::createModule());
#endif
  return Game::createGame(std::move(gameArgs));
}

void init(void) {
  //Initialize the graphics device
  sg_setup(sg_desc{
    .logger{
      .func = slog_func,
    },
    .environment = sglue_environment(),
  });
  sgl_setup(sgl_desc_t{
    .color_format = SG_PIXELFORMAT_RGBA8,
    .depth_format = SG_PIXELFORMAT_DEPTH,
    .logger{
      .func = slog_func,
    }
  });
  sfons_desc_t fd{
    .width = 1024,
    .height = 1024,
  };
  state.fontContext = sfons_create(&fd);

  AppDatabase& app = state.app;
  state.game = createGame(RendererContext{
    .swapchain = sglue_swapchain(),
    .fontContext = state.fontContext
  });

  state.game->init();

  app.combined = &state.game->getDatabase();
  app.window = &app.combined->getRuntime().query<Row<WindowData>>().get<0>(0);
  app.input.playerInput = app.combined->getRuntime().query<GameInput::PlayerKeyboardInputRow>();
  app.input.machineInput = app.combined->getRuntime().query<GameInput::StateMachineRow>();
  APP = &app;

  setWindowSize(sapp_width(), sapp_height());
}

void frame(void) {
  PROFILE_SCOPE("app", "update");
  //This is called at the monitor refresh rate. Skip draws until enough time has passed to hit no more than 60 fps
  constexpr size_t TARGET_FPS = 60;
  constexpr std::chrono::milliseconds TARGET_FRAME_TIME{ 1000 / TARGET_FPS };
  const auto now = std::chrono::steady_clock::now();
  const std::chrono::milliseconds timeSinceLastDraw = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.lastDraw);
  if (timeSinceLastDraw < TARGET_FRAME_TIME) {
    //Resubmit the last frame
    state.game->updateRendering();
    return;
  }
  state.lastDraw = now;

  state.game->updateSimulation();

  PROFILE_UPDATE(nullptr);
}

void cleanup(void) {
#ifdef IMGUI_ENABLED
  simgui_shutdown();
#endif
  sfons_destroy(state.fontContext);
  state.fontContext = nullptr;
  sgl_shutdown();
  sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
  for(int i = 0; i < argc; ++i) {
    state.args.emplace_back(argv[i]);
  }
  return sapp_desc {
    .init_cb = init,
    .frame_cb = frame,
    .cleanup_cb = cleanup,
    .event_cb = onEvent,
    //TODO: default?
    .width = 640,
    .height = 480,
    //Match monitor refresh rate one to one
    //If I knew how to ask what the refresh rate was I would use a ratio to put it at 60fps
    .swap_interval = 1,
    .window_title = "DOF",
    .icon = sapp_icon_desc{
      .sokol_default = true,
    },
    .logger = sapp_logger{
      .func = slog_func
    },
    .win32_console_create = true
  };
}
