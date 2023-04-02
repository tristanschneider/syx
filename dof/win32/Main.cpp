
#include "Precompile.h"

//For console
#include <io.h>
#include <fcntl.h>

#include <Windows.h>

#ifdef IMGUI_ENABLED
#include "ImguiModule.h"
#endif

#include "Simulation.h"
#include "TableOperations.h"
#include "Renderer.h"

#include "glm/gtx/norm.hpp"
#include "Profile.h"

struct AppDatabase {
  GameDatabase mGame;
  RendererDatabase mRenderer;
};

namespace {
  AppDatabase* APP = nullptr;
}

void setWindowSize(int width, int height) {
  WindowData& data = std::get<Row<WindowData>>(std::get<GraphicsContext>(APP->mRenderer.mTables).mRows).at(0);
  data.mWidth = width;
  data.mHeight = height;
}

void onFocusChanged(WPARAM w) {
  if(LOWORD(w) == TRUE) {
    std::get<Row<WindowData>>(std::get<GraphicsContext>(APP->mRenderer.mTables).mRows).at(0).mFocused = true;
  }
  else {
    std::get<Row<WindowData>>(std::get<GraphicsContext>(APP->mRenderer.mTables).mRows).at(0).mFocused = false;
  }
}

void onKey(WPARAM key, GameDatabase& db, KeyState state) {
  const float moveAmount = state == KeyState::Triggered ? 1.0f : 0.0f;
  const bool isDown = state == KeyState::Triggered;

  PlayerTable& players = std::get<PlayerTable>(db.mTables);
  for(size_t i = 0; i < TableOperations::size(players); ++i) {
    PlayerKeyboardInput& keyboard = std::get<Row<PlayerKeyboardInput>>(players.mRows).at(i);
    keyboard.mRawKeys.push_back(std::make_pair(state, (int)key));

    PlayerInput& input = std::get<Row<PlayerInput>>(players.mRows).at(i);
    switch(key) {
      case 'W': keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Up, isDown); break;
      case 'A':keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Left, isDown); break;
      case 'S': keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Down, isDown); break;
      case 'D': keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Right, isDown); break;
      //Set flag so input isn't missed, simulation can unset it once it's read.
      case VK_SPACE: input.mAction1 = input.mAction1 || isDown; break;
      case VK_LSHIFT: input.mAction2 = input.mAction2 || isDown; break;
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

  CameraTable& cameras = std::get<CameraTable>(db.mTables);
  for(DebugCameraControl& camera : std::get<Row<DebugCameraControl>>(cameras.mRows).mElements) {
    switch(key) {
      case VK_ADD: camera.mAdjustZoom = moveAmount; break;
      case VK_SUBTRACT: camera.mAdjustZoom = -moveAmount; break;
      case 'O': camera.mTakeSnapshot = camera.mTakeSnapshot || isDown; break;
      case 'P': camera.mLoadSnapshot = camera.mLoadSnapshot || isDown; break;
      default: break;
    }
  }
}

void enqueueMouseWheel(GameDatabase& db, float wheelDelta) {
  db;wheelDelta;
}

LRESULT onWMInput(GameDatabase& db, HWND wnd, LPARAM l) {
  PlayerTable& players = std::get<PlayerTable>(db.mTables);
  auto& keyboards = std::get<Row<PlayerKeyboardInput>>(players.mRows);
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

void resetInput(GameDatabase& db) {
  for(PlayerKeyboardInput& input : std::get<Row<PlayerKeyboardInput>>(std::get<PlayerTable>(db.mTables).mRows).mElements) {
    input.mRawKeys.clear();
    input.mRawWheelDelta = 0.0f;
    input.mRawMouseDeltaPixels = glm::vec2{ 0.0f };
  }
}

LRESULT CALLBACK mainProc(HWND wnd, UINT msg, WPARAM w, LPARAM l) {
  auto doKey = [](WPARAM w, KeyState state) {
    if(APP) {
      onKey(w, APP->mGame, state);
    }
  };
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

    case WM_LBUTTONDOWN: doKey(VK_LBUTTON, KeyState::Triggered); return 0;
    case WM_LBUTTONUP: doKey(VK_LBUTTON, KeyState::Released); return 0;
    case WM_RBUTTONDOWN: doKey(VK_RBUTTON, KeyState::Triggered); return 0;
    case WM_RBUTTONUP: doKey(VK_RBUTTON, KeyState::Released); return 0;
    case WM_MBUTTONDOWN: doKey(VK_MBUTTON, KeyState::Triggered); return 0;
    case WM_MBUTTONUP: doKey(VK_MBUTTON, KeyState::Released); return 0;
    case WM_KEYDOWN: doKey(w, KeyState::Triggered); return 0;
    case WM_KEYUP: doKey(w, KeyState::Released); return 0;

    case WM_MOUSEWHEEL:
      if(APP) {
        enqueueMouseWheel(APP->mGame, static_cast<float>(GET_WHEEL_DELTA_WPARAM(w)));
      }
      break;

    case WM_INPUT:
      if(APP && w == RIM_INPUT) {
        return onWMInput(APP->mGame, wnd, l);
      }

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

  std::string strArgs(args ? std::string(args) : std::string());
  if(!strArgs.empty()) {
    std::get<SharedRow<FileSystem>>(std::get<GlobalGameData>(APP->mGame.mTables).mRows).at().mRoot = strArgs;
  }
  else {
    std::get<SharedRow<FileSystem>>(std::get<GlobalGameData>(APP->mGame.mTables).mRows).at().mRoot = "data/";
  }

  auto lastFrameStart = std::chrono::high_resolution_clock::now();
  while(!exit) {
    PROFILE_SCOPE("app", "update");
    auto frameStart = std::chrono::high_resolution_clock::now();
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

    //TODO: scheduling
    {
      PROFILE_SCOPE("app", "simulation");
      Simulation::update(APP->mGame);
    }
    {
      PROFILE_SCOPE("app", "render");
      Renderer::render(APP->mGame, APP->mRenderer);
    }
    {
#ifdef IMGUI_ENABLED
      PROFILE_SCOPE("app", "imgui");
      static ImguiData imguidata;
      ImguiModule::update(imguidata, APP->mGame, APP->mRenderer);
#endif
    }
    {
      PROFILE_SCOPE("app", "swap");
      Renderer::swapBuffers(APP->mRenderer);
    }
    resetInput(APP->mGame);
    PROFILE_UPDATE(nullptr);

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

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
  AppDatabase app;
  //Default construct the graphcis context
  auto context = TableOperations::addToTable(std::get<GraphicsContext>(app.mRenderer.mTables));
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
  context.get<1>().mWindow = wnd;
  Renderer::initDeviceContext(context);
  Renderer::initGame(app.mGame, app.mRenderer);
  int exitCode = mainLoop(lpCmdLine);

  APP = nullptr;

  return exitCode;
}