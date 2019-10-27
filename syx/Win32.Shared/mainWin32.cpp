
#include "Precompile.h"

#define GLEW_NO_GLU
#define NOMINMAX

#include <windows.h>

//For console
#include <io.h>
#include <fcntl.h>

/*

#include "asset/Shader.h"
#include "App.h"
#include "file/FilePath.h"
#include "system/GraphicsSystem.h"
#include "system/KeyboardInput.h"
#include "win32/AppPlatformWin32.h"

#include <gl/glew.h>


using namespace Syx;

namespace {
  HDC sDeviceContext = NULL;
  HGLRC sGLContext = NULL;
  std::unique_ptr<App> sApp;
  int sWidth, sHeight;
  KeyboardInput* input = nullptr;
}

HWND gHwnd = NULL;

void setWindowSize(int width, int height) {
  sWidth = width;
  sHeight = height;
  if(sApp) {
    sApp->getSystem<GraphicsSystem>()->onResize(width, height);
  }
}

void onFocusChanged(WPARAM w) {
  if(sApp) {
    if(LOWORD(w) == TRUE)
      sApp->getAppPlatform().onFocusGained();
    else
      sApp->getAppPlatform().onFocusLost();
  }
}

void _registerDragDrop(HWND window) {
  ::DragAcceptFiles(window, TRUE);
}

LRESULT _handleDragDrop(HWND wnd, UINT msg, WPARAM w, LPARAM l) {
  const HDROP drop = reinterpret_cast<HDROP>(w);
  const UINT fileCount = ::DragQueryFileA(drop, 0xFFFFFFFF, nullptr, 0);
  std::vector<FilePath> files;

  std::array<char, FilePath::MAX_FILE_PATH> buffer;
  for(UINT i = 0; i < fileCount; ++i) {
    if(::DragQueryFileA(drop, i, buffer.data(), buffer.size())) {
      printf("Receiving file %s\n", buffer.data());
      files.emplace_back(FilePath(buffer.data()));
    }
  }

  ::DragFinish(drop);
  sApp->getAppPlatform().onDrop(files);
  return ::DefWindowProc(wnd, msg, w, l);
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

    case WM_MOUSEWHEEL:
      if(input) {
        input->feedWheelDelta(static_cast<float>(GET_WHEEL_DELTA_WPARAM(w))/static_cast<float>(WHEEL_DELTA));
      }
      break;

    case WM_DROPFILES:
      return _handleDragDrop(wnd, msg, w, l);
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

void initDeviceContext(HDC context, BYTE colorBits, BYTE depthBits, BYTE stencilBits, BYTE auxBuffers) {
  PIXELFORMATDESCRIPTOR pfd = {
    sizeof(PIXELFORMATDESCRIPTOR),
    1,
    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    //The kind of framebuffer. RGBA or palette.
    PFD_TYPE_RGBA,
    colorBits,
    0, 0, 0, 0, 0, 0,
    0,
    0,
    0,
    0, 0, 0, 0,
    depthBits,
    stencilBits,
    auxBuffers,
    PFD_MAIN_PLANE,
    0,
    0, 0, 0
  };
  //Ask for appropriate format
  int format = ChoosePixelFormat(context, &pfd);
  //Store format in device context
  SetPixelFormat(context, format, &pfd);
}

HGLRC createGLContext(HDC dc) {
  //Use format to create and opengl context
  HGLRC context = wglCreateContext(dc);
  //Make the opengl context current for this thread
  wglMakeCurrent(dc, context);
  return context;
}

void destroyContext() {
  //To destroy the context, it must be made not current
  wglMakeCurrent(sDeviceContext, NULL);
  wglDeleteContext(sGLContext);
}

void sleepNS(int ns) {
  //Would probably be best to process coroutines or something here instead of sleep
  //sleep_for is pretty erratic, so yield in a loop instead
  auto before = std::chrono::high_resolution_clock::now();
  int slept = 0;
  int yieldSlop = 300;
  while(slept + yieldSlop < ns) {
    std::this_thread::yield();
    auto after = std::chrono::high_resolution_clock::now();
    slept = static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(after - before).count());
  }
}

int mainLoop(const char* launchUri) {
  BOOL gotMessage;
  MSG msg;
  bool exit = false;
  float nsToMS = 1.0f/1000000.0f;
  float msToNS = 1000000.0f;
  int targetFrameTimeNS = 16*static_cast<int>(msToNS);
  sApp = std::make_unique<App>(std::make_unique<AppPlatformWin32>());

  sApp->init();
  sApp->onUriActivated(launchUri);
  input = sApp->getSystem<KeyboardInput>();
  //Inform graphcis of screen size
  setWindowSize(sWidth, sHeight);
  auto lastFrameStart = std::chrono::high_resolution_clock::now();
  while(!exit) {
    auto frameStart = std::chrono::high_resolution_clock::now();
    int dtNS = static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(frameStart - lastFrameStart).count());
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

    sApp->update(static_cast<float>(dtNS)*nsToMS*0.001f);
    SwapBuffers(sDeviceContext);

    int frameTimeNS = static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - frameStart).count());
    //If frame time was greater than target time then we're behind, start the next frame immediately
    int timeToNextFrameNS = targetFrameTimeNS - frameTimeNS;
    if(timeToNextFrameNS <= 0)
      continue;
    sleepNS(timeToNextFrameNS);
  }
  sApp->uninit();

  return msg.wParam;
}

void createConsole() {
  AllocConsole();
  FILE* fp;
  freopen_s(&fp, "CONOUT$", "w", stdout);
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
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

  _registerDragDrop(wnd);

  sDeviceContext = GetDC(wnd);
  initDeviceContext(sDeviceContext, 32, 24, 8, 0);
  sGLContext = createGLContext(sDeviceContext);
  glewInit();

  UpdateWindow(wnd);

  gHwnd = wnd;
  int exitCode = mainLoop(lpCmdLine);

  destroyContext();

  return exitCode;
}
*/


namespace {
  int sWidth, sHeight;
}

HWND gHwnd = NULL;

void setWindowSize(int width, int height) {
  sWidth = width;
  sHeight = height;
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
  wc.lpszMenuName = L"MainMenu";    // name of menu resource 
  wc.lpszClassName = L"MainClass";  // name of window class 
  wc.hIconSm = NULL; // small class icon 
  // Register the window class.
  RegisterClassEx(&wc);
}

int mainLoop(const char*) {
  BOOL gotMessage;
  MSG msg;
  bool exit = false;
  //Inform graphcis of screen size
  setWindowSize(sWidth, sHeight);
  auto lastFrameStart = std::chrono::high_resolution_clock::now();
  while(!exit) {
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

    //SwapBuffers(sDeviceContext);

    std::this_thread::yield();
  }
  return 0;
}

void createConsole() {
  AllocConsole();
  FILE* fp;
  freopen_s(&fp, "CONOUT$", "w", stdout);
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
  createConsole();

  registerWindow(hinstance);
  HWND wnd = CreateWindow(L"MainClass", L"SYX",
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

  //sDeviceContext = GetDC(wnd);
  //initDeviceContext(sDeviceContext, 32, 24, 8, 0);
  //sGLContext = createGLContext(sDeviceContext);
  //glewInit();

  UpdateWindow(wnd);

  gHwnd = wnd;
  int exitCode = mainLoop(lpCmdLine);

  //destroyContext();

  return exitCode;
}