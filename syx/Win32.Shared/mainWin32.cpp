
#include "Precompile.h"

//For console
#include <io.h>
#include <fcntl.h>

#include "AppPlatformWin32.h"
#include "AppRegistration.h"
#include "asset/Shader.h"
#include "App.h"
#include "ecs/component/AppPlatformComponents.h"
#include "ecs/component/UriActivationComponent.h"
#include "ecs/ECS.h"
#include "ecs/system/ogl/AssetPreviewSystemOGL.h"
#include "ecs/system/ogl/AssetSystemOGL.h"
#include "ecs/system/ogl/GraphicsSystemOGL.h"
#include "ecs/system/ogl/ImGuiSystem.h"
#include "ecs/system/PlatformMessageSystemWin32.h"
#include "ecs/system/ProjectLocatorSystem.h"
#include "ecs/system/RawInputSystemWin32.h"
#include "event/LifecycleEvents.h"
#include "file/FilePath.h"
#include "system/InputSystemWin32.h"

#include "GL/glew.h"

using namespace Syx;

struct Win32Systems : public AppRegistration {
  using SetDirView = Engine::View<Engine::Read<SetWorkingDirectoryComponent>>;
  static void tickSetCurrentDirectory(Engine::SystemContext<SetDirView>& context) {
    auto& view = context.get<SetDirView>();
    for(auto chunk = view.chunksBegin(); chunk != view.chunksEnd(); ++chunk) {
      for(const auto& setDir : *(*chunk).tryGet<const SetWorkingDirectoryComponent>()) {
        ::SetCurrentDirectoryA(setDir.mDirectory.cstr());
      }
    }
  }

  virtual void registerAppContext(Engine::AppContext& context) override {
    //Register base first then slot the platform specifics in
    mDefaultApp->registerAppContext(context);

    Engine::AppContext::PhaseContainer initializers = context.getInitializers();
    initializers.mSystems.push_back(RawInputSystemWin32::init());
    initializers.mSystems.push_back(GraphicsSystemOGL::init());
    initializers.mSystems.push_back(ImGuiSystems::init());

    Engine::AppContext::PhaseContainer input = context.getUpdatePhase(Engine::AppPhase::Input);
    //Input starts the imgui frame, render ends it
    input.mSystems.insert(input.mSystems.begin(), ImGuiSystems::updateInput());
    //Push before the input system that reads from the input event buffer that the win32 system is populating
    input.mSystems.insert(input.mSystems.begin(), RawInputSystemWin32::update());

    Engine::AppContext::PhaseContainer simulation = context.getUpdatePhase(Engine::AppPhase::Simulation);
    //Push first so more systems can respond to it
    simulation.mSystems.insert(simulation.mSystems.begin(), PlatformMessageSystemWin32::applyQueuedMessages());
    simulation.mSystems.push_back(ecx::makeSystem("SetWorkingDirectory", &tickSetCurrentDirectory));
    Engine::AppContext::PhaseContainer graphics = context.getUpdatePhase(Engine::AppPhase::Graphics);
    graphics.mSystems.insert(graphics.mSystems.begin(), AssetPreviewSystemOGL::previewTexture());
    graphics.mSystems.push_back(GraphicsSystemOGL::onWindowResized());
    graphics.mSystems.push_back(GraphicsSystemOGL::render());
    graphics.mSystems.push_back(ImGuiSystems::render());
    graphics.mSystems.push_back(GraphicsSystemOGL::swapBuffers());
    graphics.mSystems.push_back(AssetSystemOGL::uploadTextures());
    graphics.mSystems.push_back(AssetSystemOGL::uploadModels());
    graphics.mSystems.push_back(AssetSystemOGL::compileShaders());

    context.registerInitializer(std::move(initializers.mSystems));
    context.registerUpdatePhase(Engine::AppPhase::Simulation, std::move(simulation.mSystems), simulation.mTargetFPS);
    context.registerUpdatePhase(Engine::AppPhase::Input, std::move(input.mSystems), input.mTargetFPS);
    context.registerUpdatePhase(Engine::AppPhase::Graphics, std::move(graphics.mSystems), graphics.mTargetFPS);

    context.buildExecutionGraph();
  }

  //TODO: get rid of this legacy non-ecs stuff
  virtual void registerSystems(const SystemArgs& args, ISystemRegistry& registry) override {
    mDefaultApp->registerSystems(args, registry);
  }

  virtual void registerComponents(IComponentRegistry& reg) override {
    mDefaultApp->registerComponents(reg);
  }

  std::unique_ptr<AppRegistration> mDefaultApp = Registration::createDefaultApp();
};

namespace {
  std::unique_ptr<App> sApp;
  int sWidth, sHeight;
}

HWND gHwnd = NULL;

//TODO: turn into an entity with a message component that the graphics system handles
void setWindowSize(int width, int height) {
  sWidth = width;
  sHeight = height;
  PlatformMessageSystemWin32::enqueueMessage(OnWindowResizeMessageComponent{ Syx::Vec2(static_cast<float>(width), static_cast<float>(height)) });
}

void onFocusChanged(WPARAM w) {
  if(LOWORD(w) == TRUE) {
    PlatformMessageSystemWin32::enqueueMessage(OnFocusGainedMessageComponent{});
  }
  else {
    PlatformMessageSystemWin32::enqueueMessage(OnFocusLostMessageComponent{});
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
    if(::DragQueryFileA(drop, i, buffer.data(), UINT(buffer.size()))) {
      printf("Receiving file %s\n", buffer.data());
      files.emplace_back(FilePath(buffer.data()));
    }
  }

  ::DragFinish(drop);
  PlatformMessageSystemWin32::enqueueMessage(OnFilesDroppedMessageComponent{ std::move(files) });
  return ::DefWindowProc(wnd, msg, w, l);
}

LRESULT CALLBACK mainProc(HWND wnd, UINT msg, WPARAM w, LPARAM l) {
  //TODO: if there's more than one of these, make a win32 message handler interface to iterate over here.
  if(const std::optional<LRESULT> maybeResult = RawInputSystemWin32::mainProc(wnd, msg, w, l)) {
    return *maybeResult;
  }

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
  MSG msg = { 0 };
  bool exit = false;
  float nsToMS = 1.0f/1000000.0f;
  float msToNS = 1000000.0f;
  int targetFrameTimeNS = 16*static_cast<int>(msToNS);

  //Hard coded initial set of working directory so initialization can rely on a stable working directory
  if(auto setDir = ProjectLocatorSystem::tryParseSetWorkingDirectory(UriActivationComponent{ launchUri })) {
    ::SetCurrentDirectoryA(setDir->mDirectory.cstr());
  }

  sApp = std::make_unique<App>(std::make_unique<AppPlatformWin32>(), std::make_unique<Win32Systems>());

  sApp->onUriActivated(launchUri);
  sApp->init();
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

    int frameTimeNS = static_cast<int>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - frameStart).count());
    //If frame time was greater than target time then we're behind, start the next frame immediately
    int timeToNextFrameNS = targetFrameTimeNS - frameTimeNS;
    if(timeToNextFrameNS <= 0)
      continue;
    sleepNS(timeToNextFrameNS);
  }
  //TODO: deal with shutdown
  sApp.release();
  //sApp->uninit();

  return static_cast<int>(msg.wParam);
}

void createConsole() {
  AllocConsole();
  FILE* fp;
  freopen_s(&fp, "CONOUT$", "w", stdout);
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
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

  UpdateWindow(wnd);

  gHwnd = wnd;
  int exitCode = mainLoop(lpCmdLine);

  return exitCode;
}