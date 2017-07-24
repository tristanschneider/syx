#define GLEW_NO_GLU
#define NOMINMAX

#include "Precompile.h"
#include <windows.h>

//For console
#include <io.h>
#include <fcntl.h>

#include "Shader.h"
#include "App.h"

using namespace Syx;

static HDC gDeviceContext = NULL;
static HGLRC gGLContext = NULL;

void setWindowSize(int width, int height) {
  glViewport(0, 0, width, height);
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
      setWindowSize(rect.right - rect.left, rect.top - rect.bottom);
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
  wglMakeCurrent(gDeviceContext, NULL);
  wglDeleteContext(gGLContext);
}

void sleepMS(int ms) {
  //Would probably be best to process coroutines or something here instead of sleep
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int mainLoop() {
  BOOL gotMessage;
  MSG msg;
  bool exit = false;
  int targetFrameTimeMS = 16;
  App app;

  app.init();
  auto lastFrameEnd = std::chrono::high_resolution_clock::now();
  while(!exit) {
    auto frameStart = std::chrono::high_resolution_clock::now();
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

    int dtMS = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(frameStart - lastFrameEnd).count());
    app.update(static_cast<float>(dtMS)*0.001f);
    SwapBuffers(gDeviceContext);

    lastFrameEnd = std::chrono::high_resolution_clock::now();
    int frameTimeMS = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(lastFrameEnd - frameStart).count());
    //If frame time was greater than target time then we're behind, start the next frame immediately
    int timeToNextFrameMS = targetFrameTimeMS - frameTimeMS;
    if(timeToNextFrameMS <= 0)
      continue;
    sleepMS(timeToNextFrameMS);
  }
  app.uninit();

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

  gDeviceContext = GetDC(wnd);
  initDeviceContext(gDeviceContext, 32, 24, 8, 0);
  gGLContext = createGLContext(gDeviceContext);
  glewInit();

  UpdateWindow(wnd);

  int exitCode = mainLoop();

  destroyContext();

  return exitCode;
}