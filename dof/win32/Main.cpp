
#include "Precompile.h"

//For console
#include <io.h>
#include <fcntl.h>

#include <Windows.h>

#include "GameBuilder.h"
#include "GameDatabase.h"
#include "GameScheduler.h"
#include "Simulation.h"
#include "TableOperations.h"
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
  std::unique_ptr<IDatabase> combined;
  std::unique_ptr<IAppBuilder> builder;
  Row<WindowData>* window{};
  InputArgs input;
};

namespace {
  AppDatabase* APP = nullptr;
}
struct TaskGraph {
  TaskRange tasks;
  TaskRange renderCommit;
  Scheduler* scheduler{};
};

struct AppState {
  sg_pipeline pip;
  sg_bindings bind;
  sg_pass_action pass_action;
  AppDatabase app;
  TaskGraph tasks;
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

  //TODO: could be moved to game project
  mapper.addPassthroughKeyMapping(GameInput::Keys::GAME_ON_GROUND);
}

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

void resetInput(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("reset input");
  auto input = task.query<GameInput::PlayerKeyboardInputRow>();
  task.setCallback([input](AppTaskArgs&) mutable {
    input.forEachElement([](GameInput::PlayerKeyboardInput& input) {
      input.mRawKeys.clear();
      input.mRawWheelDelta = 0.0f;
      input.mRawMouseDeltaPixels = glm::vec2{ 0.0f };
      input.mRawText.clear();
    });
  });
  builder.submitTask(std::move(task));
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

TaskRange createRendererCommit() {
  std::unique_ptr<IAppBuilder> commitBuilder = GameBuilder::create(*APP->combined);
  auto temp = commitBuilder->createTask();
  temp.discard();
  ThreadLocalsInstance* tls = temp.query<ThreadLocalsRow>().tryGetSingletonElement();
  Renderer::commit(*commitBuilder);
  return GameScheduler::buildTasks(IAppBuilder::finalize(std::move(commitBuilder)), *tls->instance);
}

TaskGraph createTaskGraph() {
  auto temp = APP->builder->createTask();
  temp.discard();
  FileSystem& fs = temp.query<SharedRow<FileSystem>>().get<0>(0).at();

  //First argument is the executable location, second and beyond are arguments passed to the executable
  if(state.args.size() >= 2) {
    fs.mRoot = state.args[1];
  }
  else {
    fs.mRoot = "data/";
  }

  //First initialize just the scheduler synchronously
  std::unique_ptr<IAppBuilder> bootstrap = GameBuilder::create(*APP->combined);
  Simulation::initScheduler(*bootstrap);
  for(auto&& work : GameScheduler::buildSync(IAppBuilder::finalize(std::move(bootstrap)))) {
    work.work();
  }
  ThreadLocalsInstance* tls = temp.query<ThreadLocalsRow>().tryGetSingletonElement();
  Scheduler* scheduler = temp.query<SharedRow<Scheduler>>().tryGetSingletonElement();

  //The rest of the init can be scheduled asynchronously but still can't be done in parallel with creating the other tasks
  std::unique_ptr<IAppBuilder> initBuilder = GameBuilder::create(*APP->combined);

  Renderer::init(*initBuilder, RendererContext{
    .swapchain = sglue_swapchain(),
    .fontContext = state.fontContext
  });

  Simulation::init(*initBuilder);
  GameInput::init(*initBuilder);
  TaskRange initTasks = GameScheduler::buildTasks(IAppBuilder::finalize(std::move(initBuilder)), *tls->instance);

  setWindowSize(sapp_width(), sapp_height());

  initTasks.mBegin->mTask.addToPipe(scheduler->mScheduler);
  scheduler->mScheduler.WaitforTask(initTasks.mEnd->mTask.get());

  std::unique_ptr<IAppBuilder> builder = GameBuilder::create(*APP->combined);

  Renderer::processRequests(*builder);
  Renderer::extractRenderables(*builder);
  Renderer::clearRenderRequests(*builder);
  Renderer::render(*builder);
  Simulation::buildUpdateTasks(*builder, {});
#ifdef IMGUI_ENABLED
  ImguiModule::update(*builder);
#endif
  Renderer::endMainPass(*builder);
  Renderer::commit(*builder);
  resetInput(*builder);
  GameInput::update(*builder);
  std::shared_ptr<AppTaskNode> appTaskNodes = IAppBuilder::finalize(std::move(builder));
  constexpr bool outputGraph = false;
  if(outputGraph && appTaskNodes) {
    GraphViz::writeHere("graph.gv", *appTaskNodes);
  }
  return TaskGraph{
    .tasks = GameScheduler::buildTasks(std::move(appTaskNodes), *tls->instance),
    .renderCommit = createRendererCommit(),
    .scheduler = scheduler
  };
}

std::unique_ptr<IDatabase> createDatabase() {
  auto mappings = std::make_unique<StableElementMappings>();
  std::unique_ptr<IDatabase> game = GameDatabase::create(*mappings);
  std::unique_ptr<IAppBuilder> tempBuilder = GameBuilder::create(*game);
  auto tempA = tempBuilder->createTask();
  tempA.discard();
  auto tempB = tempBuilder->createTask();
  tempB.discard();
  std::unique_ptr<IDatabase> renderer = Renderer::createDatabase(std::move(tempA), *mappings);
  std::unique_ptr<IDatabase> result = DBReflect::merge(std::move(game), std::move(renderer));
#ifdef IMGUI_ENABLED
  result = DBReflect::merge(std::move(result), ImguiModule::createDatabase(std::move(tempB), *mappings));
#endif
  result = DBReflect::bundle(std::move(result), std::move(mappings));
  return result;
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
  app.combined = createDatabase();
  app.window = &app.combined->getRuntime().query<Row<WindowData>>().get<0>(0);
  app.builder = GameBuilder::create(*app.combined);
  app.input.playerInput = app.combined->getRuntime().query<GameInput::PlayerKeyboardInputRow>();
  app.input.machineInput = app.combined->getRuntime().query<GameInput::StateMachineRow>();
  APP = &app;

  Input::InputMapper* mapper = app.combined->getRuntime().query<GameInput::GlobalMappingsRow>().tryGetSingletonElement();
  createKeyboardMappings(*mapper);

  state.tasks = createTaskGraph();
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
    state.tasks.renderCommit.mBegin->mTask.addToPipe(state.tasks.scheduler->mScheduler);
    state.tasks.scheduler->mScheduler.WaitforTask(state.tasks.renderCommit.mEnd->mTask.get());
    return;
  }
  state.lastDraw = now;

  state.tasks.tasks.mBegin->mTask.addToPipe(state.tasks.scheduler->mScheduler);
  state.tasks.scheduler->mScheduler.WaitforTask(state.tasks.tasks.mEnd->mTask.get());

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
