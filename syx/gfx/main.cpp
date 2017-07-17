#define GLEW_NO_GLU
#define NOMINMAX

#include <gl/glew.h>

#include <windows.h>
#include <stdio.h>
#include <string>
#include <algorithm>
#include <chrono>
#include <thread>
#include <fstream>

//For console
#include <io.h>
#include <fcntl.h>

#include <SyxMat4.h>
#include <SyxVec3.h>
#include <SyxQuat.h>
#include <SyxMat3.h>
#include "Shader.h"

using namespace Syx;

HDC deviceContext = NULL;
HGLRC glContext = NULL;
Shader gShader;

void readFile(const std::string& path, std::string& buffer) {
  std::ifstream file(path, std::ifstream::in | std::ifstream::binary);
  if(file.good()) {
    file.seekg(0, file.end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, file.beg);
    buffer.resize(size + 1);
    file.read(&buffer[0], size);
    buffer[size] = 0;
  }
}

Shader loadShadersFromFile(const std::string& vsPath, const std::string& psPath) {
  Shader result;
  std::string vs, ps;
  readFile(vsPath, vs);
  readFile(psPath, ps);
  if(vs.size() && ps.size()) {
    result.load(vs, ps);
  }
  return result;
}

static GLuint vertexBuffer;
static GLuint vertexArray;

void _createTestTriangle() {
  //Generate a vertex array name
  glGenVertexArrays(1, &vertexArray);
  //Bind this array so we can fill it in
  glBindVertexArray(vertexArray);
  GLfloat triBuff[] = {
      -1.0f, -1.0f, 0.0f,
      1.0f, -1.0f, 0.0f,
      0.0f,  1.0f, 0.0f,
  };
  //Generate a vertex buffer name
  glGenBuffers(1, &vertexBuffer);
  //Bind vertexBuffer as "Vertex attributes"
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  //Upload triBuff to gpu as vertexBuffer
  glBufferData(GL_ARRAY_BUFFER, sizeof(triBuff), triBuff, GL_STATIC_DRAW);
}

void initGraphics() {
  _createTestTriangle();
  gShader = loadShadersFromFile("shaders/phong.vs", "shaders/phong.ps");
}

void render() {
  glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  //Specify that we're talking about the zeroth attribute
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
  //Define the attribute structure
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

  {
    Shader::Binder b(gShader);

    static Vec3 camPos(0.0f, 0.0f, -3.0f);
    Mat4 camTransform = Mat4::transform(Vec3::Identity, Quat::LookAt(-Vec3::UnitZ), camPos);
    static Quat triRot = Quat::Identity;
    triRot *= Quat::AxisAngle(Vec3::UnitY, 0.01f);
    Mat4 triTransform = Mat4::transform(Vec3::Identity, triRot, Vec3::Zero);
    float fov = 1.396f;
    float nearD = 0.1f;
    float farD = 100.0f;
    Mat4 proj = Mat4::perspective(fov, fov, nearD, farD);
    Mat4 mvp = proj * camTransform.affineInverse() * triTransform.affineInverse();

    glUniformMatrix4fv(gShader.getUniform("mvp"), 1, GL_FALSE, mvp.mData);

    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
  glDisableVertexAttribArray(0);

  SwapBuffers(deviceContext);
}

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
  wglMakeCurrent(deviceContext, NULL);
  wglDeleteContext(glContext);
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

    render();
    auto frameEnd = std::chrono::high_resolution_clock::now();
    int frameTimeMS = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart).count());
    //If frame time was greater than target time then we're behind, start the next frame immediately
    int timeToNextFrameMS = targetFrameTimeMS - frameTimeMS;
    if(timeToNextFrameMS <= 0)
      continue;
    sleepMS(timeToNextFrameMS);
  }

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

  deviceContext = GetDC(wnd);
  initDeviceContext(deviceContext, 32, 24, 8, 0);
  glContext = createGLContext(deviceContext);
  glewInit();

  initGraphics();

  UpdateWindow(wnd);

  int exitCode = mainLoop();

  destroyContext();

  return exitCode;
}