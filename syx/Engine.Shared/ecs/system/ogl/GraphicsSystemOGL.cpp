#include "Precompile.h"
#include "ecs/system/ogl/GraphicsSystemOGL.h"

#include "ecs/component/AssetComponent.h"
#include "ecs/component/GraphicsComponents.h"
#include "ecs/component/ogl/OGLContextComponent.h"
#include "ecs/component/ogl/OGLHandleComponents.h"
#include "ecs/component/PlatformMessageComponents.h"
#include "ecs/component/ScreenSizeComponent.h"
#include "ecs/component/TransformComponent.h"

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
    Engine::Entity mPhongShader;
    Engine::Entity mFullScreenQuadShader;
    Engine::Entity mFlatColorShader;
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

  Engine::Entity _queueShaderProgramLoad(const FilePath& vsPath, const FilePath& psPath, EntityFactory& factory) {
    auto&& [ vs, vsReq ] = factory.createAndGetEntityWithComponents<AssetLoadRequestComponent>();
    vsReq.get().mPath = vsPath;
    auto&& [ ps, psReq ] = factory.createAndGetEntityWithComponents<AssetLoadRequestComponent>();
    psReq.get().mPath = psPath;
    auto&& [ result, program, n ] = factory.createAndGetEntityWithComponents<ShaderProgramComponent, NeedsGpuUploadComponent>();
    program.get().mPixelShader = ps;
    program.get().mVertexShader = vs;
    return result;
  }

  void tickInit(SystemContext<EntityFactory, OGLView>& systemContext) {
    EntityFactory factory = systemContext.get<EntityFactory>();
    auto&& [entity, ctx] = factory.createAndGetEntityWithComponents<OGLContext>();

    OGLContext& context = ctx.get();
    context.mDeviceContext = GetDC(gHwnd);
    initDeviceContext(context.mDeviceContext, 32, 24, 8, 0);
    context.mGLContext = createGLContext(ctx.get().mDeviceContext);
    glewInit();

    //Init is called on the graphics thread, move the context from the platform created thread to this
    //TODO: less goofy once context creation is here
    if(!wglMakeCurrent(ctx.get().mDeviceContext, ctx.get().mGLContext)) {
      //TODO: how to deal with this?
      printf(("Failed to create context + " + std::to_string(GetLastError())).c_str());
    }

    context.mPhongShader = _queueShaderProgramLoad("shaders/phong.vs", "shaders/phong.ps", factory);
    context.mFlatColorShader = _queueShaderProgramLoad("shaders/flatColor.vs", "shaders/flatColor.ps", factory);
    context.mFullScreenQuadShader = _queueShaderProgramLoad("shaders/fullScreenQuad.vs", "shaders/fullScreenQuad.ps", factory);

    const char* versionGL =  (char*)glGetString(GL_VERSION);
    printf("version %s", versionGL);

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
  using ShaderProgramView = View<Include<ShaderProgramComponent>, Include<AssetComponent>, Read<ShaderProgramHandleOGLComponent>>;
  using RenderablesView = View<Read<GraphicsModelRefComponent>, Read<TextureRefComponent>, Read<TransformComponent>>;
  using ModelsView = View<Read<GraphicsModelHandleOGLComponent>>;
  using TexturesView = View<Read<TextureHandleOGLComponent>>;
  using ViewportView = View<Read<ViewportComponent>>;
  using CameraView = View<Read<CameraComponent>, Read<TransformComponent>>;

  void tickRender(SystemContext<ContextView,
    ScreenSizeView,
    ShaderProgramView,
    OGLView,
    RenderablesView,
    ModelsView,
    TexturesView,
    ViewportView,
    CameraView>& context) {
    auto ogl = context.get<ContextView>().tryGetFirst();
    auto screen = context.get<ScreenSizeView>().tryGetFirst();
    if(!ogl || !screen) {
      return;
    }

    auto& modelsView = context.get<ModelsView>();
    auto& texturesView = context.get<TexturesView>();

    for(auto viewEntity : context.get<ViewportView>()) {
      const ViewportComponent& viewportComponent = viewEntity.get<const ViewportComponent>();
      Viewport viewport({}, Syx::Vec2(viewportComponent.mMinX, viewportComponent.mMinY), Syx::Vec2(viewportComponent.mMaxX, viewportComponent.mMaxY));

      _glViewport(viewport, screen->get<const ScreenSizeComponent>().mScreenSize);
      //Write clear color before any early outs due to missing components
      glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

      auto& renderables = context.get<RenderablesView>();
      if(!renderables.tryGetFirst()) {
        return;
      }
      CameraView& cameraView = context.get<CameraView>();
      auto camera = cameraView.find(viewportComponent.mCamera);
      if(camera == cameraView.end()) {
        continue;
      }
      const TransformComponent& cameraTransform = (*camera).get<const TransformComponent>();
      const CameraComponent& cameraComponent = (*camera).get<const CameraComponent>();

      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);

      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);

      auto& shaders = context.get<ShaderProgramView>();
      const OGLContext& oglContext = ogl->get<OGLContext>();
      auto phong = shaders.find(oglContext.mPhongShader);
      if(phong == shaders.end()) {
        break;
      }
      const ShaderProgramHandleOGLComponent& program = (*phong).get<const ShaderProgramHandleOGLComponent>();

      glUseProgram(program.mProgram);

      const Syx::Vec3 camPos(cameraTransform.mValue.getTranslate());
      const Syx::Mat4 wvp = Syx::Mat4::perspective(cameraComponent.mFOVX, cameraComponent.mFOVY, cameraComponent.mNear, cameraComponent.mFar) * cameraTransform.mValue.affineInverse();
      const Syx::Vec3 mDiff(1.0f);
      const Syx::Vec3 mSpec(0.6f, 0.6f, 0.6f, 2.5f);
      const Syx::Vec3 mAmb(0.22f, 0.22f, 0.22f);
      const Syx::Vec3 sunDir = -Syx::Vec3::Identity.normalized();
      const Syx::Vec3 sunColor = Syx::Vec3::Identity;

      auto cp = glGetUniformLocation(program.mProgram, "uCamPos");
      auto diffuse = glGetUniformLocation(program.mProgram, "uDiffuse");
      auto ambient = glGetUniformLocation(program.mProgram, "uAmbient");
      auto specular = glGetUniformLocation(program.mProgram, "uSpecular");
      auto sunDirU = glGetUniformLocation(program.mProgram, "uSunDir");
      auto sunColorU = glGetUniformLocation(program.mProgram, "uSunColor");
      auto texU = glGetUniformLocation(program.mProgram, "uTex");
      auto mvpU = glGetUniformLocation(program.mProgram, "uMVP");
      auto mwU = glGetUniformLocation(program.mProgram, "uMW");

      glUniform3f(cp, camPos.x, camPos.y, camPos.z);
      glUniform3f(diffuse, mDiff.x, mDiff.y, mDiff.z);
      glUniform3f(ambient, mAmb.x, mAmb.y, mAmb.z);
      glUniform4f(specular, mSpec.x, mSpec.y, mSpec.z, mSpec.w);
      glUniform3f(sunDirU, sunDir.x, sunDir.y, sunDir.z);
      glUniform3f(sunColorU, sunColor.x, sunColor.y, sunColor.z);

      //TODO: sort to reduce individual binds
      for(auto chunk : renderables.chunks()) {
        for(size_t i = 0; i < chunk.size(); ++i) {
          const std::vector<TransformComponent>* transforms = chunk.tryGet<const TransformComponent>();
          const std::vector<TextureRefComponent>* textures = chunk.tryGet<const TextureRefComponent>();
          const std::vector<GraphicsModelRefComponent>* models = chunk.tryGet<const GraphicsModelRefComponent>();
          for(size_t t = 0; t < transforms->size(); ++t) {
            auto texture = texturesView.find(textures->at(t).mTexture);
            auto model = modelsView.find(models->at(t).mModel);
            //TODO: exclude entities with missing textures or models
            if(texture == texturesView.end() || model == modelsView.end()) {
              continue;
            }

            glActiveTexture(GL_TEXTURE0);
            auto texID = (*texture).get<const TextureHandleOGLComponent>().mTexture;
            glBindTexture(GL_TEXTURE_2D, texID);
            //TODO: can probably be above with other uniforms if this doesn't require bound texture
            glUniform1i(texU, 0);

            //TODO: flag entities when they move so this can be precomputed
            const Syx::Mat4 mw = transforms->at(t).mValue;
            const Syx::Mat4 mvp = wvp * mw;
            glUniformMatrix4fv(mvpU, 1, GL_FALSE, mvp.mData);
            glUniformMatrix4fv(mwU, 1, GL_FALSE, mw.mData);

            const auto& modelGL = (*model).get<const GraphicsModelHandleOGLComponent>();
            glBindVertexArray(modelGL.mVertexArray);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, modelGL.mIndexBuffer);

            glDrawElements(GL_TRIANGLES, GLsizei(modelGL.mIndexCount), GL_UNSIGNED_INT, nullptr);
          }
        }
      }

      //Unbind affer all looping is done
      glBindVertexArray(0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      glBindTexture(GL_TEXTURE_2D, 0);

      glUseProgram(0);
    }
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
