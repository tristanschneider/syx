
#include "Precompile.h"

//For console
#include <io.h>
#include <fcntl.h>

#include <Windows.h>

#include "Simulation.h"
#include "TableOperations.h"
#include "Renderer.h"

#include "glm/gtx/norm.hpp"

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

enum class KeyState : uint8_t {
  Triggered,
  Released
};

void onKey(WPARAM key, GameDatabase& db, KeyState state) {
  const float moveAmount = state == KeyState::Triggered ? 1.0f : 0.0f;
  const bool isDown = state == KeyState::Triggered;

  PlayerTable& players = std::get<PlayerTable>(db.mTables);
  for(size_t i = 0; i < TableOperations::size(players); ++i) {
    PlayerKeyboardInput& keyboard = std::get<Row<PlayerKeyboardInput>>(players.mRows).at(i);
    PlayerInput& input = std::get<Row<PlayerInput>>(players.mRows).at(i);
    switch(key) {
      case 'W': keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Up, isDown); break;
      case 'A':keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Left, isDown); break;
      case 'S': keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Down, isDown); break;
      case 'D': keyboard.mKeys.set((size_t)PlayerKeyboardInput::Key::Right, isDown); break;
      case VK_SPACE: input.mAction1 = isDown; break;
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
      default: break;
    }
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

    case WM_KEYDOWN:
      if(APP) {
        onKey(w, APP->mGame, KeyState::Triggered);
      }
      return 0;

    case WM_KEYUP:
      if(APP) {
        onKey(w, APP->mGame, KeyState::Released);
      }
      return 0;
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

int mainLoop(const char*) {
  BOOL gotMessage;
  MSG msg = { 0 };
  bool exit = false;
  float msToNS = 1000000.0f;
  int targetFrameTimeNS = 16*static_cast<int>(msToNS);

  auto lastFrameStart = std::chrono::high_resolution_clock::now();
  while(!exit) {
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
    Simulation::update(APP->mGame);
    Renderer::render(APP->mGame, APP->mRenderer);

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

  context.get<1>().mWindow = wnd;
  Renderer::initDeviceContext(context);
  int exitCode = mainLoop(lpCmdLine);

  APP = nullptr;

  return exitCode;
}