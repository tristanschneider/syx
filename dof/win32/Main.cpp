
#include "Precompile.h"

//For console
#include <io.h>
#include <fcntl.h>

#include <Windows.h>

#ifdef IMGUI_ENABLED
#include "ImguiModule.h"
#endif

#include "GameBuilder.h"
#include "GameDatabase.h"
#include "GameScheduler.h"
#include "Simulation.h"
#include "TableOperations.h"
#include "ThreadLocals.h"
#include "Renderer.h"

#include "glm/gtx/norm.hpp"
#include "Profile.h"

using InputQueryResult = QueryResult<Row<PlayerKeyboardInput>, Row<PlayerInput>>;

struct InputArgs {
  InputQueryResult input;
  QueryResult<Row<DebugCameraControl>> debugCamera;
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

void setWindowSize(int width, int height) {
  WindowData& data = APP->window->at(0);
  data.mWidth = width;
  data.mHeight = height;
}

void onFocusChanged(WPARAM w) {
  WindowData& data = APP->window->at(0);
  if(LOWORD(w) == TRUE) {
    data.mFocused = true;
  }
  else {
    data.mFocused = false;
  }
}

void onKey(WPARAM key, InputArgs& args, KeyState state) {
  const float moveAmount = state == KeyState::Triggered ? 1.0f : 0.0f;
  const bool isDown = state == KeyState::Triggered;

  for(size_t i = 0; i < args.input.size(); ++i) {
    PlayerKeyboardInput& keyboard = args.input.get<0>(0).at(i);
    keyboard.mRawKeys.push_back(std::make_pair(state, (int)key));

    PlayerInput& input = args.input.get<1>(0).at(i);
    switch(key) {
      case 'W': keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Up, isDown); break;
      case 'A':keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Left, isDown); break;
      case 'S': keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Down, isDown); break;
      case 'D': keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Right, isDown); break;
      case VK_SPACE: input.mAction1 = state; break;
      case VK_LSHIFT: input.mAction2 = state; break;
      default: break;
    }
    input.mMoveX = input.mMoveY = 0.0f;
    if(keyboard.mKeys.test((size_t)PlayerKeyboardInput::Key::Up)) {
      input.mMoveY += 1.0f;
    }
    if(keyboard.mKeys.test((size_t)PlayerKeyboardInput::Key::Down)) {
      input.mMoveY -= 1.0f;
    }
    if(keyboard.mKeys.test((size_t)PlayerKeyboardInput::Key::Left)) {
      input.mMoveX -= 1.0f;
    }
    if(keyboard.mKeys.test((size_t)PlayerKeyboardInput::Key::Right)) {
      input.mMoveX += 1.0f;
    }
    glm::vec2 normalized = glm::vec2(input.mMoveX, input.mMoveY);
    if(float len = glm::length(normalized); len > 0.0001f) {
      normalized /= len;
      input.mMoveX = normalized.x;
      input.mMoveY = normalized.y;
    }
  }

  args.debugCamera.forEachElement([&](DebugCameraControl& camera) {
    switch(key) {
      case VK_ADD: camera.mAdjustZoom = moveAmount; break;
      case VK_SUBTRACT: camera.mAdjustZoom = -moveAmount; break;
      case 'O': camera.mTakeSnapshot = camera.mTakeSnapshot || isDown; break;
      case 'P': camera.mLoadSnapshot = camera.mLoadSnapshot || isDown; break;
      default: break;
    }
  });
}

void enqueueMouseWheel(InputArgs& db, float wheelDelta) {
  db;wheelDelta;
}

LRESULT onWMInput(InputArgs& args, HWND wnd, LPARAM l) {
  auto& keyboards = args.input.get<0>(0);
  if(!keyboards.size()) {
    return 0;
  }
  PlayerKeyboardInput& keyboard = keyboards.at(0);

  std::array<uint8_t, 1024> buffer;
  UINT bufferSize = static_cast<UINT>(buffer.size());
  if(::GetRawInputData(reinterpret_cast<HRAWINPUT>(l), RID_INPUT, buffer.data(), &bufferSize, sizeof(RAWINPUTHEADER))) {
    RAWINPUT* input = reinterpret_cast<RAWINPUT*>(buffer.data());

    //Currently only registered for mouse events anyway with RegisterRawInputDevices
    if(input->header.dwType == RIM_TYPEMOUSE) {
      const RAWMOUSE& mouseInput = input->data.mouse;
      glm::vec2 mousePos{ 0.0f };
      glm::vec2 mouseDelta{ 0.0f };
      std::optional<bool> newIsRelative;
      //This is a bitfield but RELATIVE is 0, so I'm assuming that relative movement is never combined with other values
      if(mouseInput.usFlags == MOUSE_MOVE_RELATIVE) {
        newIsRelative = true;
        mouseDelta = { static_cast<float>(mouseInput.lLastX), static_cast<float>(mouseInput.lLastY) };
        POINT result;
        ::GetCursorPos(&result);
        mousePos = { static_cast<float>(result.x), static_cast<float>(result.y) };
      }
      else if(mouseInput.usFlags & MOUSE_MOVE_ABSOLUTE) {
        newIsRelative = false;
        mousePos = { static_cast<float>(mouseInput.lLastX), static_cast<float>(mouseInput.lLastY) };
      }

      if(newIsRelative) {
        POINT p{ (LONG)mousePos.x, (LONG)mousePos.y };
        if(::ScreenToClient(wnd, &p)) {
          mousePos.x = (float)p.x;
          mousePos.y = (float)p.y;
        }

        //If relative mode was the same as last time, compute the delta normally
        if(*newIsRelative == keyboard.mIsRelativeMouse) {
          if(!keyboard.mIsRelativeMouse) {
            mouseDelta = mousePos - keyboard.mLastMousePos;
            keyboard.mLastMousePos = mousePos;
          }
        }
        //Relative mouse mode changed, don't compute a delta as it may be dramatic, set last pos for potential use next input
        else {
          keyboard.mLastMousePos = mousePos;
          keyboard.mIsRelativeMouse = *newIsRelative;
        }
      }
      keyboard.mRawMousePixels = mousePos;
      keyboard.mRawMouseDeltaPixels = mouseDelta;
    }

    //Documentation says this does nothing. Call it because it seems reasonable
    ::DefRawInputProc(&input, 1, sizeof(RAWINPUTHEADER));
  }
  //Mark this event as handled
  return 0;
}

void onChar(InputArgs& args, WPARAM w) {
  auto& keyboards = args.input.get<0>(0);
  if(!keyboards.size()) {
    return;
  }
  PlayerKeyboardInput& keyboard = keyboards.at(0);

  if(std::isprint(static_cast<int>(w))) {
    keyboard.mRawText.push_back(static_cast<char>(w));
  }
}

void resetInput(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("reset input");
  auto input = task.query<Row<PlayerKeyboardInput>>();
  task.setCallback([input](AppTaskArgs&) mutable {
    input.forEachElement([](PlayerKeyboardInput& input) {
      input.mRawKeys.clear();
      input.mRawWheelDelta = 0.0f;
      input.mRawMouseDeltaPixels = glm::vec2{ 0.0f };
      input.mRawText.clear();
    });
  });
}

void doKey(WPARAM w, LPARAM l) {
  if(!APP) {
    return;
  }
  const WORD keyFlags = HIWORD(l);
  const BOOL wasKeyDown = (keyFlags & KF_REPEAT) == KF_REPEAT;
  const BOOL isKeyReleased = (keyFlags & KF_UP) == KF_UP;
  if(isKeyReleased) {
    onKey(w, APP->input, KeyState::Released);
  }
  //Only send if key wasn't already down, thereby ignoring repeat inputs
  else if(!wasKeyDown) {
    onKey(w, APP->input, KeyState::Triggered);
  }
}

void doMouseKey(WPARAM w, KeyState state) {
  if(APP) {
    onKey(w, APP->input, state);
  }
}

LRESULT CALLBACK mainProc(HWND wnd, UINT msg, WPARAM w, LPARAM l) {
  switch(msg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    case WM_SIZE:
      setWindowSize(LOWORD(l), HIWORD(l));
      break;

    case WM_SIZING: {
      RECT& rect = *reinterpret_cast<RECT*>(l);
      setWindowSize(rect.right - rect.left, rect.bottom - rect.top);
      break;
    }

    case WM_ACTIVATEAPP:
      onFocusChanged(w);
      break;

    case WM_LBUTTONDOWN:
      doMouseKey(VK_LBUTTON, KeyState::Triggered);
      return 0;
    case WM_LBUTTONUP:
      doMouseKey(VK_LBUTTON, KeyState::Released);
      return 0;
    case WM_RBUTTONDOWN:
      doMouseKey(VK_RBUTTON, KeyState::Triggered);
      return 0;
    case WM_RBUTTONUP:
      doMouseKey(VK_RBUTTON, KeyState::Released);
      return 0;
    case WM_MBUTTONDOWN:
      doMouseKey(VK_MBUTTON, KeyState::Triggered);
      return 0;
    case WM_MBUTTONUP:
      doMouseKey(VK_MBUTTON, KeyState::Released);
      return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
      doKey(w, l);
      return 0;

    case WM_MOUSEWHEEL:
      if(APP) {
        enqueueMouseWheel(APP->input, static_cast<float>(GET_WHEEL_DELTA_WPARAM(w)));
      }
      break;

    case WM_INPUT:
      if(APP && w == RIM_INPUT) {
        return onWMInput(APP->input, wnd, l);
      }
      break;

    case WM_CHAR:
      if(APP) {
        onChar(APP->input, w);
      }
      break;
  }
  return DefWindowProc(wnd, msg, w, l);
}

void registerWindow(HINSTANCE inst) {
  WNDCLASSEX wc;
  wc.cbSize = sizeof(wc);          // size of structure 
  wc.style = CS_HREDRAW | CS_VREDRAW; // redraw if size changes 
  wc.lpfnWndProc = mainProc;     // points to window procedure 
  wc.cbClsExtra = 0;                // no extra class memory 
  wc.cbWndExtra = 0;                // no extra window memory 
  wc.hInstance = inst;         // handle to instance 
  wc.hIcon = NULL; // predefined app. icon 
  wc.hCursor = LoadCursor(NULL, IDC_ARROW); // predefined arrow 
  wc.hbrBackground = CreateSolidBrush(COLOR_ACTIVEBORDER);
  wc.lpszMenuName = "MainMenu";    // name of menu resource 
  wc.lpszClassName = "MainClass";  // name of window class 
  wc.hIconSm = NULL; // small class icon 
  // Register the window class.
  RegisterClassEx(&wc);
}

void sleepNS(int ns) {
  //Would probably be best to process coroutines or something here instead of sleep
  //sleep_for is pretty erratic, so yield in a loop instead
  auto before = std::chrono::high_resolution_clock::now();
  int slept = 0;
  int yieldSlop = 300;
  while(slept + yieldSlop < ns) {
    auto remaining = ns - slept;
    if(remaining > static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(1)).count())) {
      std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    else {
      std::this_thread::yield();
    }
    auto after = std::chrono::high_resolution_clock::now();
    slept = static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(after - before).count());
  }
}

int mainLoop(const char* args) {
  BOOL gotMessage;
  MSG msg = { 0 };
  bool exit = false;
  float msToNS = 1000000.0f;
  int targetFrameTimeNS = 16*static_cast<int>(msToNS);

  auto temp = APP->builder->createTask();
  temp.discard();
  FileSystem& fs = temp.query<SharedRow<FileSystem>>().get<0>(0).at();

  std::string strArgs(args ? std::string(args) : std::string());
  if(!strArgs.empty()) {
    fs.mRoot = strArgs;
  }
  else {
    fs.mRoot = "data/";
  }

  //First initialize just the scheudler synchronously
  std::unique_ptr<IAppBuilder> bootstrap = GameBuilder::create(*APP->combined);
  Simulation::initScheduler(*bootstrap);
  for(auto&& work : GameScheduler::buildSync(IAppBuilder::finalize(std::move(bootstrap)))) {
    work.work();
  }
  ThreadLocalsInstance* tls = temp.query<ThreadLocalsRow>().tryGetSingletonElement();
  Scheduler* scheduler = temp.query<SharedRow<Scheduler>>().tryGetSingletonElement();

  //The rest of the init can be scheduled asynchronously but still can't be done in parallel with creating the other tasks
  std::unique_ptr<IAppBuilder> initBuilder = GameBuilder::create(*APP->combined);
  Simulation::init(*initBuilder);
  TaskRange initTasks = GameScheduler::buildTasks(IAppBuilder::finalize(std::move(initBuilder)), *tls->instance);

  initTasks.mBegin->mTask.addToPipe(scheduler->mScheduler);
  scheduler->mScheduler.WaitforTask(initTasks.mEnd->mTask.get());

  std::unique_ptr<IAppBuilder> builder = GameBuilder::create(*APP->combined);

  Renderer::processRequests(*builder);
  Renderer::clearRenderRequests(*builder);
  Renderer::extractRenderables(*builder);

  Simulation::buildUpdateTasks(*builder);

  //gameplay
  Renderer::render(*builder);
#ifdef IMGUI_ENABLED
  ImguiModule::update(*builder);
#endif
  resetInput(*builder);
  Renderer::swapBuffers(*builder);

  TaskRange appTasks = GameScheduler::buildTasks(IAppBuilder::finalize(std::move(builder)), *tls->instance);

  auto lastFrameStart = std::chrono::high_resolution_clock::now();
  while(!exit) {
    std::chrono::steady_clock::time_point frameStart;
    {
      PROFILE_SCOPE("app", "update");
      frameStart = std::chrono::high_resolution_clock::now();
      lastFrameStart = frameStart;

      while((gotMessage = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) > 0) {
        if(msg.message == WM_QUIT) {
          exit = true;
          break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      if(exit)
        break;

      appTasks.mBegin->mTask.addToPipe(scheduler->mScheduler);
      scheduler->mScheduler.WaitforTask(appTasks.mEnd->mTask.get());
      PROFILE_UPDATE(nullptr);
    }

    int frameTimeNS = static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - frameStart).count());
    //If frame time was greater than target time then we're behind, start the next frame immediately
    int timeToNextFrameNS = targetFrameTimeNS - frameTimeNS;
    if(timeToNextFrameNS <= 0)
      continue;
    sleepNS(timeToNextFrameNS);
  }

  return static_cast<int>(msg.wParam);
}

void createConsole() {
  AllocConsole();
  FILE* fp;
  freopen_s(&fp, "CONOUT$", "w", stdout);
}

void enableRawMouseInput() {
  std::array<RAWINPUTDEVICE, 1> devices;
  auto& mouse = devices[0];
  mouse.usUsagePage = 1;
  mouse.usUsage = 2;
  //The examples show using RIDEV_NOLEGACY but those events are needed for normal window interaction, so keep the flags empty
  mouse.dwFlags = 0;
  mouse.hwndTarget = 0;

  if(!RegisterRawInputDevices(devices.data(), static_cast<UINT>(devices.size()), sizeof(RAWINPUTDEVICE))) {
    const DWORD err = GetLastError();
    printf("Failed to register input with error %s\n", std::to_string(err).c_str());
  }
}

std::unique_ptr<IDatabase> createDatabase() {
  auto mappings = std::make_unique<StableElementMappings>();
  std::unique_ptr<IDatabase> game = GameData::create(*mappings);
  std::unique_ptr<IAppBuilder> tempBuilder = GameBuilder::create(*game);
  std::unique_ptr<IDatabase> renderer = Renderer::createDatabase(tempBuilder->createTask(), *mappings);
  std::unique_ptr<IDatabase> result = DBReflect::merge(std::move(game), std::move(renderer));
#ifdef IMGUI_ENABLED
  result = DBReflect::merge(std::move(result), ImguiModule::createDatabase(tempBuilder->createTask(), *mappings));
#endif
  result = DBReflect::bundle(std::move(result), std::move(mappings));
  return result;
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
  AppDatabase app;
  app.combined = createDatabase();
  app.window = &app.combined->getRuntime().query<Row<WindowData>>().get<0>(0);
  app.builder = GameBuilder::create(*app.combined);
  app.input.input = app.combined->getRuntime().query<Row<PlayerKeyboardInput>, Row<PlayerInput>>();
  app.input.debugCamera = app.combined->getRuntime().query<Row<DebugCameraControl>>();
  APP = &app;

  createConsole();

  registerWindow(hinstance);
  HWND wnd = CreateWindow("MainClass", "SYX",
    WS_OVERLAPPEDWINDOW | CS_OWNDC,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    NULL,
    NULL,
    hinstance,
    NULL);
  ShowWindow(wnd, nCmdShow);

  UpdateWindow(wnd);

  enableRawMouseInput();
  Renderer::initDeviceContext(wnd, app.builder->createTask());
  Renderer::initGame(app.builder->createTask());
  int exitCode = mainLoop(lpCmdLine);

  APP = nullptr;

  return exitCode;
}