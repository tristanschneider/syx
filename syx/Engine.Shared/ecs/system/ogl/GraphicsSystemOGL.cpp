#include "Precompile.h"
#include "ecs/system/ogl/GraphicsSystemOGL.h"

#include "ecs/component/ogl/OGLContextComponent.h"
#include "ecs/component/PlatformMessageComponents.h"
#include "ecs/component/ScreenSizeComponent.h"

#include "GL/glew.h"
#include "graphics/FrameBuffer.h"
#include "graphics/PixelBuffer.h"
#include "graphics/Viewport.h"

#include <Windows.h>

//TODO: move context and window creation here
extern HWND gHwnd;


namespace OGLImpl {
  constexpr size_t GRAPHICS_THREAD = 0;

  using namespace Engine;

  struct OGLContext {
    std::unique_ptr<FrameBuffer> mFrameBuffer;
    std::unique_ptr<PixelBuffer> mPixelPackBuffer;
    HGLRC mGLContext{};
    HDC mDeviceContext{};
  };

  using ContextView = View<Write<OGLContext>>;
  using OGLView = View<Write<OGLContextComponent>>;

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

  void tickInit(SystemContext<EntityFactory, OGLView>& context) {
    auto&& [entity, ctx] = context.get<EntityFactory>().createAndGetEntityWithComponents<OGLContext>();

    ctx.get().mDeviceContext = GetDC(gHwnd);
    initDeviceContext(ctx.get().mDeviceContext, 32, 24, 8, 0);
    ctx.get().mGLContext = createGLContext(ctx.get().mDeviceContext);
    glewInit();


    //Init is called on the graphics thread, move the context from the platform created thread to this
    //TODO: less goofy once context creation is here
    if(!wglMakeCurrent(ctx.get().mDeviceContext, ctx.get().mGLContext)) {
      //TODO: how to deal with this?
      printf(("Failed to create context + " + std::to_string(GetLastError())).c_str());
    }
  }

  //TODO: Graphics system destroy this?
  //void destroyContext() {
  //  //To destroy the context, it must be made not current
  //  wglMakeCurrent(gDeviceContext, NULL);
  //  wglDeleteContext(gGLContext);
  //}

  using WindowResizeView = View<Read<OnWindowResizeMessageComponent>>;
  void tickWindowResize(SystemContext<ContextView, WindowResizeView, OGLView>& context) {
    for(auto it : context.get<WindowResizeView>()) {
      auto global = context.get<ContextView>().tryGetFirst();
      if(!global) {
        return;
      }
      auto& oglContext = global->get<OGLContext>();

      const Syx::Vec2& size = it.get<const OnWindowResizeMessageComponent>().mNewSize;
      glViewport(0, 0, (GLsizei)size.x, (GLsizei)size.y);

      TextureDescription desc((GLsizei)size.x, (GLsizei)size.y, TextureFormat::RGBA8, TextureSampleMode::Nearest);
      oglContext.mFrameBuffer = std::make_unique<FrameBuffer>(desc);
      oglContext.mPixelPackBuffer = std::make_unique<PixelBuffer>(desc, PixelBuffer::Type::Pack);
    }
  }

  void _glViewport(const Viewport& viewport, const Syx::Vec2& screenSize) {
    glViewport(static_cast<int>(viewport.getMin().x*screenSize.x),
      static_cast<int>(viewport.getMin().y*screenSize.y),
      static_cast<int>((viewport.getMax().x - viewport.getMin().x)*screenSize.x),
      static_cast<int>((viewport.getMax().y - viewport.getMin().y)*screenSize.y)
    );
  }

  using ScreenSizeView = View<Read<ScreenSizeComponent>>;

  void tickRender(SystemContext<ContextView, ScreenSizeView, OGLView>& context) {
    auto ogl = context.get<ContextView>().tryGetFirst();
    auto screen = context.get<ScreenSizeView>().tryGetFirst();
    if(!ogl || !screen) {
      return;
    }

    //TODO: viewport component and camera
    Viewport viewport({}, Syx::Vec2(0.f), Syx::Vec2(1.f));
    _glViewport(viewport, screen->get<const ScreenSizeComponent>().mScreenSize);
    glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    //TODO: actual rendering
  }

  void tickSwapBuffers(SystemContext<ContextView, OGLView>& context) {
    if(auto ogl = context.get<ContextView>().tryGetFirst()) {
      SwapBuffers(ogl->get<OGLContext>().mDeviceContext);
    }
  }
}

std::shared_ptr<Engine::System> GraphicsSystemOGL::init() {
  return ecx::makeSystem("OGLInit", &OGLImpl::tickInit, OGLImpl::GRAPHICS_THREAD);
}

std::shared_ptr<Engine::System> GraphicsSystemOGL::onWindowResized() {
  return ecx::makeSystem("OGLResize", &OGLImpl::tickWindowResize, OGLImpl::GRAPHICS_THREAD);
}

std::shared_ptr<Engine::System> GraphicsSystemOGL::render() {
  return ecx::makeSystem("OGLRender", &OGLImpl::tickRender, OGLImpl::GRAPHICS_THREAD);
}

std::shared_ptr<Engine::System> GraphicsSystemOGL::swapBuffers() {
  return ecx::makeSystem("OGLSwap", &OGLImpl::tickSwapBuffers, OGLImpl::GRAPHICS_THREAD);
}
