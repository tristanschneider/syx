#include "Precompile.h"
#include "Renderer.h"

#include "GL/glew.h"

namespace {
  void _initDevice(HDC context, BYTE colorBits, BYTE depthBits, BYTE stencilBits, BYTE auxBuffers) {
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
}

void Renderer::initDeviceContext(GraphicsContext::ElementRef& context) {
  OGLState& state = context.get<0>();
  WindowData& window = context.get<1>();

  state.mDeviceContext = GetDC(window.mWindow);
  _initDevice(state.mDeviceContext, 32, 24, 8, 0);
  state.mGLContext = createGLContext(state.mDeviceContext);
  glewInit();

  const char* versionGL =  (char*)glGetString(GL_VERSION);
  printf("version %s", versionGL);
}

void Renderer::render(GameDatabase&, RendererDatabase& renderDB) {
  OGLState& state = std::get<Row<OGLState>>(std::get<GraphicsContext>(renderDB.mTables).mRows).at(0);
  glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  SwapBuffers(state.mDeviceContext);
}
