#include "Precompile.h"
#include "Renderer.h"

#include "CommonTasks.h"
#include "Queries.h"
#include "RendererTableAdapters.h"
#include "STBInterface.h"
#include "Table.h"

#include "glm/mat3x3.hpp"
#include "glm/ext/matrix_float3x3.hpp"

#include "glm/gtx/transform.hpp"
#include "ParticleRenderer.h"
#include "Debug.h"

namespace {
  struct QuadShader {
    static constexpr const char* vs = R"(
      #version 330 core
      layout(location = 0) in vec2 aPosition;

      uniform mat4 uWorldToView;
      uniform samplerBuffer uPosX;
      uniform samplerBuffer uPosY;
      uniform samplerBuffer uPosZ;
      uniform samplerBuffer uRotX;
      uniform samplerBuffer uRotY;
      uniform samplerBuffer uUV;
      uniform samplerBuffer uTint;

      out vec2 oUV;
      out vec4 oTint;

      void main() {
        //TODO: should build transform on CPU to avoid per-vertex waste
        int i = gl_InstanceID;
        float cosAngle = texelFetch(uRotX, i).r;
        float sinAngle = texelFetch(uRotY, i).r;
        vec4 pos = vec4(aPosition, 0, 1);
        //2d rotation matrix multiply
        pos.xy = vec2(pos.x*cosAngle - pos.y*sinAngle,
          pos.x*sinAngle + pos.y*cosAngle);
        pos.xyz += vec3(
          texelFetch(uPosX, i).r,
          texelFetch(uPosY, i).r,
          texelFetch(uPosZ, i).r
        );

        gl_Position = uWorldToView*pos;

        //Order of uUV data is uMin, vMin, uMax, vMax
        //Order of vertices in quad is:
        //1   5 4
        //2 0   3
        //Manually figure out which vertex this is to pick out the uv
        //An easier way would be duplicating the uv data per vertex
        vec4 uvData = texelFetch(uUV, i);
        vec2 uvMin = uvData.xy;
        vec2 uvMax = uvData.zw;
        switch(gl_VertexID) {
          //Bottom right
          case 0:
          case 3:
            oUV = vec2(uvMax.x, uvMin.y);
            break;

          //Top right
          case 4:
            oUV = uvMax;
            break;

          //Top left
          case 1:
          case 5:
            oUV = vec2(uvMin.x, uvMax.y);
            break;

          //Bottom left
          case 2:
            oUV = uvMin;
            break;
        }

        oTint = texelFetch(uTint, i);
      }
      )";
    static constexpr const char* ps = R"(
        #version 330 core

        uniform sampler2D uTex;

        in vec2 oUV;
        in vec4 oTint;
        layout(location = 0) out vec3 color;

        void main() {
          //uv flip hack to make the texture look right. uvs are bottom left to top right, picture is loaded top left to bottom right
          color = texture(uTex, vec2(oUV.x, 1.0-oUV.y)).rgb;
          color = mix(oTint.rgb, color.rgb, 1.0 - oTint.a);
        }
    )";
  };

  struct DebugShader {
    static constexpr const char* vs = R"(
      #version 330 core
      layout(location = 0) in vec2 aPosition;
      layout(location = 1) in vec3 aColor;
      uniform mat4 wvp;
      out vec3 vertColor;
      void main() {
        gl_Position = wvp * vec4(aPosition.xy, 0.0, 1.0);
        vertColor = aColor;
      }
    )";

    static constexpr const char* ps = R"(
      #version 330 core
      in vec3 vertColor;
      out vec3 oColor;
      void main() {
        oColor = vertColor;
      }
    )";
  };

  glm::mat4 _getWorldToView(const RendererCamera& camera, float aspectRatio) {
    //TODO: not sure why this is inverse as glm says it wants x/y which is what the ratio is
    //Maybe I'm undoing it with a different transformation like part of glviewport or otherwise
    const float inverseAspect = 1.0f / aspectRatio;
    auto proj = camera.camera.orthographic ? glm::ortho(-inverseAspect, inverseAspect, -1.0f, 1.0f)
      : glm::perspective(glm::radians(camera.camera.fovDeg), inverseAspect, camera.camera.nearPlane, camera.camera.farPlane);
    //TODO: should be able to factor this into the view rather than needing a separate scale
    glm::vec3 scale = glm::vec3(camera.camera.zoom);
    return glm::inverse(
      glm::translate(glm::vec3(camera.pos.x, camera.pos.y, 0.0f)) *
      glm::rotate(camera.camera.angle, glm::vec3(0, 0, -1)) *
      glm::scale(scale) *
      proj
    );
  }

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

  GLuint _createQuadBuffers() {
    GLuint result;
    const float verts[] = { 0.5f, -0.5f,
      -0.5f, 0.5f,
      -0.5f, -0.5f,

      0.5f, -0.5f,
      0.5f, 0.5f,
      -0.5f, 0.5f
    };
    //Generate and upload vertex buffer
    glGenBuffers(1, &result);
    glBindBuffer(GL_ARRAY_BUFFER, result);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*6*2, verts, GL_STATIC_DRAW);
    return result;
  }

  RenderDebugDrawer _createDebugDrawer() {
    RenderDebugDrawer drawer;
    drawer.mShader = Shader::loadShader(DebugShader::vs, DebugShader::ps);

    drawer.mWVPUniform = glGetUniformLocation(drawer.mShader, "wvp");

    glGenBuffers(1, &drawer.mVBO);
    glBindBuffer(GL_ARRAY_BUFFER, drawer.mVBO);
    glGenVertexArrays(1, &drawer.mVAO);
    glBindVertexArray(drawer.mVAO);

    glEnableVertexAttribArray(0);
    //Position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(DebugPoint), nullptr);
    //Color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DebugPoint), reinterpret_cast<void*>(sizeof(float)*2));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return drawer;
  }

  QuadUniforms _createQuadUniforms(GLuint quadShader) {
    QuadUniforms result;
    result.posX = Shader::_createTextureSamplerUniform(quadShader, "uPosX");
    result.posY = Shader::_createTextureSamplerUniform(quadShader, "uPosY");
    result.posZ = Shader::_createTextureSamplerUniform(quadShader, "uPosZ");
    result.rotX = Shader::_createTextureSamplerUniform(quadShader, "uRotX");
    result.rotY = Shader::_createTextureSamplerUniform(quadShader, "uRotY");
    result.tint = Shader::_createTextureSamplerUniform(quadShader, "uTint");
    result.uv = Shader::_createTextureSamplerUniform(quadShader, "uUV");
    result.worldToView = glGetUniformLocation(quadShader, "uWorldToView");
    result.texture = glGetUniformLocation(quadShader, "uTex");
    result.velX = Shader::_createTextureSampler();
    result.velY = Shader::_createTextureSampler();
    result.angVel = Shader::_createTextureSampler();
    return result;
  }

  struct TexturesTuple {
    Row<TextureGLHandle>* glHandle{};
    Row<TextureGameHandle>* gameHandle{};
  };

  void _loadTexture(TextureLoadRequest& request, TexturesTuple textures, ITableModifier& texturesModifier) {
    //Don't care about alpha for the moment but images get skewed strangely unless loaded as 4 components
    ImageData data = STBInterface::loadImageFromFile(request.mFileName.c_str(), 4);
    if(!data.mBytes) {
      request.mStatus = RequestStatus::Failed;
      return;
    }

    GLuint textureHandle;
    glGenTextures(1, &textureHandle);
    glBindTexture(GL_TEXTURE_2D, textureHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(data.mWidth), GLsizei(data.mHeight), 0, GL_RGBA, GL_UNSIGNED_BYTE, data.mBytes);
    //Define sampling mode, no mip maps snap to nearest
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    size_t element = texturesModifier.addElements(1);
    textures.glHandle->at(element).mHandle = textureHandle;
    textures.gameHandle->at(element).mID = request.mImageID;

    request.mStatus = RequestStatus::Succeeded;

    STBInterface::deallocate(std::move(data));
  }

  void _processRequests(QueryResult<Row<TextureLoadRequest>>& requests,
    QueryResult<Row<TextureGLHandle>, Row<TextureGameHandle>>& textures,
    ITableModifier& texturesModifier) {

    //Table should always exist and only one
    TexturesTuple tex{ std::get<0>(textures.rows).at(0), std::get<1>(textures.rows).at(0) };
    requests;textures;texturesModifier;
    requests.forEachElement([&](TextureLoadRequest& request) {
      if(request.mStatus == RequestStatus::InProgress) {
        _loadTexture(request, tex, texturesModifier);
      }
    });
  }

  GLuint _getTextureByID(size_t id, QueryResult<const Row<TextureGameHandle>, const Row<TextureGLHandle>>& textures) {
    for(size_t i = 0; i < textures.size(); ++i) {
      const auto& handles = textures.get<0>(i).mElements;
      for(size_t j = 0; j < handles.size(); ++j) {
        if(handles[j].mID == id) {
          return textures.get<1>(i).at(j).mHandle;
        }
      }
    }
    return {};
  }

  void _bindTextureSamplerUniform(const TextureSamplerUniform& sampler, GLenum format, int index) {
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_BUFFER, sampler.texture);
    glTexBuffer(GL_TEXTURE_BUFFER, format, sampler.buffer);
    glUniform1i(sampler.uniformID, index);
  }
}

namespace {
  void debugCallback(GLenum source,
            GLenum type,
            GLuint id,
            GLenum severity,
            GLsizei length,
            const GLchar *message,
            const void *userParam) {
    source;type;id;severity;length;message;userParam;
    switch(severity) {
    case GL_DEBUG_SEVERITY_MEDIUM:
    case GL_DEBUG_SEVERITY_NOTIFICATION:
      return;
    default:
      printf("Error [%s]\n", message);
      break;
    }
  }
}

struct RenderDB : IDatabase {
  using TexturesTable = Table<
    Row<TextureGLHandle>,
    Row<TextureGameHandle>
  >;

  using GraphicsContext = Table<
    Row<OGLState>,
    Row<WindowData>
  >;

  using RendererDatabase = Database<
    GraphicsContext,
    TexturesTable,
    DebugLinePassTable::PointsTable,
    DebugLinePassTable::TextTable
  >;

  RenderDB(size_t quadPassCount, StableElementMappings& mappings)
    : runtime(getArgs(quadPassCount, mappings)) {
  }

  RuntimeDatabase& getRuntime() override {
    return runtime;
  }

  RuntimeDatabaseArgs getArgs(size_t quadPassCount, StableElementMappings& mappings) {
    RuntimeDatabaseArgs result;
    DBReflect::reflect(main, result, mappings);
    quadPasses.resize(quadPassCount);
    for(QuadPassTable::Type& pass : quadPasses) {
      DBReflect::addTable(pass, result, mappings);
    }
    return result;
  }

  RendererDatabase main;
  std::vector<QuadPassTable::Type> quadPasses;
  RuntimeDatabase runtime;
};

std::unique_ptr<IDatabase> Renderer::createDatabase(RuntimeDatabaseTaskBuilder&& builder, StableElementMappings& mappings) {
  auto sprites = builder.query<const Row<CubeSprite>>();
  const size_t quadPassCount = sprites.matchingTableIDs.size();
  //Create the database with the required number of quad pass tables
  auto result = std::make_unique<RenderDB>(quadPassCount, mappings);
  return result;
}

namespace Renderer {
  void initDeviceContext(HWND window, IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("initDeviceContexet").setPinning(AppTaskPinning::MainThread{});
    auto q = task.query<Row<OGLState>, Row<WindowData>>();
    auto modifier = task.getModifierForTable(q.matchingTableIDs.front());

    task.setCallback([q, modifier, window](AppTaskArgs&) mutable {
      const size_t ctx = 0;
      modifier->resize(1);

      OGLState& state = q.get<0>(0).at(ctx);
      WindowData& windowData = q.get<1>(0).at(ctx);
      windowData.mWindow = window;
      WINDOWINFO windowInfo{ 0 };
      windowInfo.cbSize = sizeof(WINDOWINFO);
      if(GetWindowInfo(window, &windowInfo)) {
        windowData.mHeight = windowInfo.rcClient.bottom - windowInfo.rcClient.top;
        windowData.mWidth = windowInfo.rcClient.right - windowInfo.rcClient.left;
        //TODO: set focused or there's probably a bug if the window starts unfocused
      }
      else {
        printf("Falied to initialize window size\n");
      }

      state.mDeviceContext = GetDC(windowData.mWindow);
      _initDevice(state.mDeviceContext, 32, 24, 8, 0);
      state.mGLContext = createGLContext(state.mDeviceContext);
      glewInit();
    
      glEnable(GL_DEBUG_OUTPUT);
      glDebugMessageCallback(&debugCallback, nullptr);
      const char* versionGL =  (char*)glGetString(GL_VERSION);
      printf("version %s", versionGL);
    
      state.mQuadShader = Shader::loadShader(QuadShader::vs, QuadShader::ps);
      state.mQuadVertexBuffer = _createQuadBuffers();
    
      state.mDebug = _createDebugDrawer();
    });

    builder.submitTask(std::move(task));
  }

  void initGame(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("renderer initGame").setPinning(AppTaskPinning::MainThread{});
    auto sprites = task.query<const Row<CubeSprite>>();
    auto resolver = task.getResolver<const IsImmobile>();
    auto quadPasses = task.query<QuadPassTable::Pass, QuadPassTable::IsImmobile>();
    auto oglState = task.query<Row<OGLState>>();
    task.setCallback([sprites, resolver, quadPasses, oglState](AppTaskArgs&) mutable {
      OGLState& ogl = oglState.get<0>(0).at(0);
      //Should match based on createDatabase resizing to this
      assert(sprites.matchingTableIDs.size() == quadPasses.matchingTableIDs.size());
    
      //Fill in the quad pass tables
      for(size_t i = 0 ; i < sprites.matchingTableIDs.size(); ++i) {
        quadPasses.get<0>(i).at().mQuadUniforms = _createQuadUniforms(ogl.mQuadShader);
        quadPasses.get<1>(i).at() = resolver->tryGetRow<const IsImmobile>(sprites.matchingTableIDs[i]) != nullptr;
      }
    });

    builder.submitTask(std::move(task));
  }
}

void Renderer::init(IAppBuilder& builder, HWND window) {
  initDeviceContext(window, builder);
  initGame(builder);
}

void _renderDebug(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("renderDebug").setPinning(AppTaskPinning::MainThread{});
  auto globals = task.query<Row<OGLState>, const Row<WindowData>>();
  auto debugLines = task.query<const DebugLinePassTable::Points>();
  task.setCallback([globals, debugLines](AppTaskArgs&) mutable {
    PROFILE_SCOPE("renderer", "debug");
    OGLState* state = globals.tryGetSingletonElement<0>();
    const WindowData* window = globals.tryGetSingletonElement<1>();
    if(!state || !window) {
      return;
    }

    RenderDebugDrawer& debug = state->mDebug;
    for(size_t i = 0; i < debugLines.size(); ++i) {
      const auto& linesToDraw = debugLines.get<0>(i);
      if(linesToDraw.size()) {
        glBindBuffer(GL_ARRAY_BUFFER, debug.mVBO);
        if(debug.mLastSize < linesToDraw.size()) {
          glBufferData(GL_ARRAY_BUFFER, sizeof(DebugPoint)*linesToDraw.size(), nullptr, GL_DYNAMIC_DRAW);
          debug.mLastSize = linesToDraw.size();
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(DebugPoint)*linesToDraw.size(), linesToDraw.mElements.data());
        glBindVertexArray(debug.mVAO);
        glUseProgram(debug.mShader);
        for(const auto& renderCamera : state->mCameras) {
          glm::mat4 worldToView = _getWorldToView(renderCamera, window->aspectRatio);
          glUniformMatrix4fv(debug.mWVPUniform, 1, GL_FALSE, &worldToView[0][0]);

          glDrawArrays(GL_LINES, 0, linesToDraw.size());
        }

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
      }
    }
    glGetError();
  });
  builder.submitTask(std::move(task));
}

void Renderer::extractRenderables(IAppBuilder& builder) {
  auto temp = builder.createTask();
  temp.discard();
  auto sharedTextureSprites = temp.query<const Row<CubeSprite>, const SharedRow<TextureReference>>();
  auto globals = temp.query<Row<OGLState>>();
  auto passes = temp.query<QuadPassTable::Pass>();
  assert(sharedTextureSprites.size() == passes.size());

  //Quads
  for(size_t pass = 0; pass < passes.size(); ++pass) {
    const UnpackedDatabaseElementID& passID = passes.matchingTableIDs[pass];
    const UnpackedDatabaseElementID& spriteID = sharedTextureSprites.matchingTableIDs[pass];

    //Resize the quad pass table to match the size of its paired sprite table
    {
      auto task = builder.createTask();
      task.setName("Resize Renderables");
      std::shared_ptr<ITableModifier> modifier = task.getModifierForTable(passes.matchingTableIDs[pass]);
      auto query = task.query<const Row<CubeSprite>>(spriteID);
      task.setCallback([modifier, query](AppTaskArgs&) mutable {
        modifier->resize(query.get<0>(0).size());
      });
      builder.submitTask(std::move(task));
    }
    //Copy each row of data to resized table
    CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::Pos, Tags::X>, QuadPassTable::PosX>(builder, spriteID, passID);
    CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::Pos, Tags::Y>, QuadPassTable::PosY>(builder, spriteID, passID);
    CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::Rot, Tags::CosAngle>, QuadPassTable::RotX>(builder, spriteID, passID);
    CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::Rot, Tags::SinAngle>, QuadPassTable::RotY>(builder, spriteID, passID);
    CommonTasks::moveOrCopyRowSameSize<Row<CubeSprite>, QuadPassTable::UV>(builder, spriteID, passID);
    CommonTasks::moveOrCopyRowSameSize<SharedRow<TextureReference>, QuadPassTable::Texture>(builder, spriteID, passID);
    if(builder.queryTable<Tags::PosZRow>(spriteID)) {
      CommonTasks::moveOrCopyRowSameSize<Tags::PosZRow, QuadPassTable::PosZ>(builder, spriteID, passID);
    }
    //If this table has velocity, add those tasks as well
    if(temp.query<FloatRow<Tags::LinVel, Tags::X>, FloatRow<Tags::AngVel, Tags::Angle>>(spriteID).size()) {
      CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::LinVel, Tags::X>, QuadPassTable::LinVelX>(builder, spriteID, passID);
      CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::LinVel, Tags::Y>, QuadPassTable::LinVelY>(builder, spriteID, passID);
      CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::AngVel, Tags::Angle>, QuadPassTable::AngVel>(builder, spriteID, passID);
    }
    //Tint is also optional
    if(temp.query<Tint>(spriteID).size()) {
      CommonTasks::moveOrCopyRowSameSize<Tint, QuadPassTable::Tint>(builder, spriteID, passID);
    }
  }

  //Debug lines
  {
    auto task = builder.createTask();
    task.setName("extract debug");
    auto src = task.query<const Row<DebugPoint>>();
    auto dst = task.query<DebugLinePassTable::Points>();
    assert(src.size() == dst.size());
    auto modifiers = task.getModifiersForTables(dst.matchingTableIDs);
    task.setCallback([src, dst, modifiers](AppTaskArgs&) mutable {
      for(size_t i = 0; i < modifiers.size(); ++i) {
        auto& srcRow = src.get<0>(i);
        modifiers[i]->resize(srcRow.size());
        CommonTasks::Now::moveOrCopyRow(srcRow, dst.get<0>(i), 0);
      }
    });
    builder.submitTask(std::move(task));
  }
  //Debug text
  {
    auto task = builder.createTask();
    task.setName("extract text");
    auto src = task.query<const Row<DebugText>>();
    auto dst = task.query<DebugLinePassTable::Texts>();
    assert(src.size() == dst.size());
    auto modifiers = task.getModifiersForTables(dst.matchingTableIDs);
    task.setCallback([src, dst, modifiers](AppTaskArgs&) mutable {
      for(size_t i = 0; i < modifiers.size(); ++i) {
        auto& srcRow = src.get<0>(i);
        auto& dstRow = dst.get<0>(i);
        modifiers[i]->resize(srcRow.size());
        for(size_t e = 0; e < srcRow.size(); ++e) {
          const DebugText& srcE = srcRow.at(e);
          DebugLinePassTable::Text& dstE = dstRow.at(e);
          dstE.pos = srcE.pos;
          dstE.text = srcE.text;
        }
      }
    });
    builder.submitTask(std::move(task));
  }

  //Cameras
  {
    auto task = builder.createTask();
    auto src = task.query<const Row<Camera>,
      const FloatRow<Tags::Pos, Tags::X>,
      const FloatRow<Tags::Pos, Tags::Y>>();
    auto dst = task.query<Row<OGLState>>();
    assert(dst.size() == 1);
    task.setName("extract cameras");
    task.setCallback([src, dst](AppTaskArgs&) mutable {
      //Lazy since presumably there's just one
      OGLState* state = dst.tryGetSingletonElement();
      if(!state) {
        return;
      }
      state->mCameras.clear();
      src.forEachElement([state](const Camera& camera, const float& posX, const float& posY) {
        state->mCameras.push_back({ glm::vec2{ posX, posY }, camera });
      });
    });
    builder.submitTask(std::move(task));
  }

  //Globals
  {
    auto task = builder.createTask();
    auto src = task.query<const SharedRow<SceneState>>();
    auto dst = task.query<Row<OGLState>>();
    assert(dst.size() == 1);
    task.setName("extract globals");
    task.setCallback([src, dst](AppTaskArgs&) mutable {
      const SceneState* scene = src.tryGetSingletonElement();
      OGLState* state = dst.tryGetSingletonElement();
      if(scene && state) {
        state->mSceneState = *scene;
      }
    });
    builder.submitTask(std::move(task));
  }
}

void Renderer::clearRenderRequests(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("clear render requests");
  auto modifiers = task.getModifiersForTables(builder.queryTables<DebugClearPerFrame>().matchingTableIDs);
  task.setCallback([modifiers](AppTaskArgs&) {
    for(auto&& modifier : modifiers) {
      modifier->resize(0);
    }
  });

  builder.submitTask(std::move(task));
}

void Renderer::processRequests(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("Process Render Requests");
  auto requests = task.query<Row<TextureLoadRequest>>();
  auto textures = task.query<Row<TextureGLHandle>, Row<TextureGameHandle>>();
  std::shared_ptr<ITableModifier> modifier{ task.getModifierForTable(textures.matchingTableIDs[0]) };
  task.setPinning(AppTaskPinning::MainThread{});
  task.setCallback([=](AppTaskArgs&) mutable {
    _processRequests(requests, textures, *modifier);
  });
  builder.submitTask(std::move(task).finalize());
}

void Renderer::render(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("Render");
  auto globals = task.query<Row<OGLState>, Row<WindowData>>();
  auto quads = task.query<const QuadPassTable::PosX, const QuadPassTable::PosY, const QuadPassTable::PosZ,
    const QuadPassTable::RotX, const QuadPassTable::RotY,
    const QuadPassTable::UV,
    const QuadPassTable::Texture,
    const QuadPassTable::LinVelX, const QuadPassTable::LinVelY,
    const QuadPassTable::AngVel,
    const QuadPassTable::Tint,
    QuadPassTable::Pass>();
  auto textures = task.query<const Row<TextureGameHandle>, const Row<TextureGLHandle>>();

  task.setPinning(AppTaskPinning::MainThread{});
  task.setCallback([globals, quads, textures](AppTaskArgs&) mutable {
    OGLState* state = globals.tryGetSingletonElement<0>();
    WindowData* window = globals.tryGetSingletonElement<1>();
    if(!state || !window) {
      return;
    }

    glGetError();

    static bool first = true;
    static ParticleData data;
    static size_t frameIndex{};
    static DebugRenderData debug = Debug::init();
    ++frameIndex;
    if(first) {
      ParticleRenderer::init(data);
      first = false;
    }

    glViewport(0, 0, window->mWidth, window->mHeight);
    const float aspectRatio = window->mHeight ? float(window->mWidth)/float(window->mHeight) : 1.0f;
    window->aspectRatio = aspectRatio;

    glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    auto& cameras = state->mCameras;
    for(auto& camera : cameras) {
      camera.worldToView = _getWorldToView(camera, aspectRatio);
    }

    for(const auto& renderCamera : cameras) {
      PROFILE_SCOPE("renderer", "particles");

      ParticleUniforms uniforms;
      uniforms.worldToView = renderCamera.worldToView;
      const SceneState& scene = state->mSceneState;
      const glm::vec2 origin = (scene.mBoundaryMin + scene.mBoundaryMax) * 0.5f;
      const float extraSpaceScalar = 2.5f;
      //*2 because particle space is NDC, meaning scale of 2: [-1,1]
      const glm::vec2 scale = (scene.mBoundaryMax - scene.mBoundaryMin) * 0.5f * extraSpaceScalar;
      uniforms.particleToWorld = glm::translate(glm::vec3(origin.x, origin.y, 0.0f)) * glm::scale(glm::vec3(scale.x, scale.y, 1.0f));
      uniforms.worldToParticle = glm::inverse(uniforms.particleToWorld);

      ParticleRenderer::renderNormalsBegin(data);
      //Render particles first so that quads can stomp them and take precedence
      ParticleRenderer::renderParticleNormals(data, uniforms, frameIndex);
      ParticleRenderer::renderQuadNormalsBegin(data);

      for(size_t i = 0; i < quads.size(); ++i) {
        const QuadPass& pass = quads.get<QuadPassTable::Pass>(i).at();
        //Hack to use the last count since it renders before the buffers are updated by quad drawing.
        if(pass.mLastCount) {
          CubeSpriteInfo info;
          info.count = pass.mLastCount;
          info.posX = pass.mQuadUniforms.posX;
          info.posY = pass.mQuadUniforms.posY;
          info.posZ = pass.mQuadUniforms.posZ;
          info.quadVertexBuffer = state->mQuadVertexBuffer;
          info.rotX = pass.mQuadUniforms.rotX;
          info.rotY = pass.mQuadUniforms.rotY;
          info.velX = pass.mQuadUniforms.velX;
          info.velY = pass.mQuadUniforms.velY;
          info.velA = pass.mQuadUniforms.angVel;

          ParticleRenderer::renderNormals(data, uniforms, info);
        }
      }

      ParticleRenderer::renderQuadNormalsEnd();
      ParticleRenderer::renderNormalsEnd();

      ParticleRenderer::update(data, uniforms, frameIndex);
      glViewport(0, 0, window->mWidth, window->mHeight);
      ParticleRenderer::render(data, uniforms, frameIndex);
    }

    glUseProgram(state->mQuadShader);
    for(const auto& renderCamera : cameras) {
      PROFILE_SCOPE("renderer", "geometry");
      for(size_t i = 0; i < quads.size(); ++i) {
        //auto passAdapter = RendererTableAdapters::getQuadPass(passTable);
        auto& posX = quads.get<const QuadPassTable::PosX>(i);
        auto& posY = quads.get<const QuadPassTable::PosY>(i);
        auto& posZ = quads.get<const QuadPassTable::PosZ>(i);
        auto& rotationX = quads.get<const QuadPassTable::RotX>(i);
        auto& rotationY = quads.get<const QuadPassTable::RotY>(i);
        auto& sprite = quads.get<const QuadPassTable::UV>(i);
        auto& texture = quads.get<const QuadPassTable::Texture>(i);
        auto& velX = quads.get<const QuadPassTable::LinVelX>(i);
        auto& velY = quads.get<const QuadPassTable::LinVelY>(i);
        auto& velA = quads.get<const QuadPassTable::AngVel>(i);
        auto& tint = quads.get<const QuadPassTable::Tint>(i);
        QuadPass& pass = quads.get<QuadPassTable::Pass>(i).at();

        size_t count = posX.size();
        pass.mLastCount = count;
        if(!count) {
          continue;
        }

        GLuint oglTexture = _getTextureByID(texture.at(), textures);
        if(!oglTexture) {
          continue;
        }

        const size_t floatSize = sizeof(float)*count;
        glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.posX.buffer);
        glBufferData(GL_TEXTURE_BUFFER, floatSize, posX.mElements.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.posY.buffer);
        glBufferData(GL_TEXTURE_BUFFER, floatSize, posY.mElements.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.posZ.buffer);
        glBufferData(GL_TEXTURE_BUFFER, floatSize, posZ.mElements.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.rotX.buffer);
        glBufferData(GL_TEXTURE_BUFFER, floatSize, rotationX.mElements.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.rotY.buffer);
        glBufferData(GL_TEXTURE_BUFFER, floatSize, rotationY.mElements.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.velX.buffer);
        glBufferData(GL_TEXTURE_BUFFER, floatSize, velX.mElements.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.velY.buffer);
        glBufferData(GL_TEXTURE_BUFFER, floatSize, velY.mElements.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.angVel.buffer);
        glBufferData(GL_TEXTURE_BUFFER, floatSize, velA.mElements.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.tint.buffer);
        glBufferData(GL_TEXTURE_BUFFER, floatSize*4, tint.mElements.data(), GL_STATIC_DRAW);

        //TODO: doesn't change every frame
        glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.uv.buffer);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(float)*count*4, sprite.mElements.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, state->mQuadVertexBuffer);
        //Could tie these to a vao, but that would also require and index buffer all of which seems like overkill
        QuadVertexAttributes::bind();

        const glm::mat4 worldToView = renderCamera.worldToView;

        glUniformMatrix4fv(pass.mQuadUniforms.worldToView, 1, GL_FALSE, &worldToView[0][0]);
        int textureIndex = 0;
        _bindTextureSamplerUniform(pass.mQuadUniforms.posX, GL_R32F, textureIndex++);
        _bindTextureSamplerUniform(pass.mQuadUniforms.posY, GL_R32F, textureIndex++);
        _bindTextureSamplerUniform(pass.mQuadUniforms.posZ, GL_R32F, textureIndex++);
        _bindTextureSamplerUniform(pass.mQuadUniforms.rotX, GL_R32F, textureIndex++);
        _bindTextureSamplerUniform(pass.mQuadUniforms.rotY, GL_R32F, textureIndex++);
        _bindTextureSamplerUniform(pass.mQuadUniforms.uv, GL_RGBA32F, textureIndex++);
        _bindTextureSamplerUniform(pass.mQuadUniforms.tint, GL_RGBA32F, textureIndex++);
        glActiveTexture(GL_TEXTURE0 + textureIndex);
        glBindTexture(GL_TEXTURE_2D, oglTexture);
        glUniform1i(pass.mQuadUniforms.texture, textureIndex++);

        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, GLsizei(count));
      }
    }

    /* TODO: move to simulation with imgui setting
    static bool renderBorders = true;
    if(renderBorders) {
      auto& debugTable = std::get<DebugLineTable>(db.mTables);
      const SceneState& scene = Simulation::_getSceneState(db);
      const glm::vec2 bl = scene.mBoundaryMin;
      const glm::vec2 ul = glm::vec2(scene.mBoundaryMin.x, scene.mBoundaryMax.y);
      const glm::vec2 ur = scene.mBoundaryMax;
      const glm::vec2 br = glm::vec2(scene.mBoundaryMax.x, scene.mBoundaryMin.y);

      std::array points{ bl, ul, ul, ur, ur, br, br, bl };
      for(const auto& p : points) {
        TableOperations::addToTable(debugTable).get<0>().mPos = p;
      }
    }
    */

    //Debug::pictureInPicture(debug, { 50, 50 }, { 350, 350 }, data.mSceneTexture);
    glGetError();
  });
  builder.submitTask(std::move(task));

  _renderDebug(builder);
}

void Renderer::swapBuffers(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("swapBuffers").setPinning(AppTaskPinning::MainThread{});
  auto query = task.query<Row<OGLState>>();
  task.setCallback([query](AppTaskArgs&) mutable {
    PROFILE_SCOPE("renderer", "swap");
    if(const OGLState* gl = query.tryGetSingletonElement()) {
      SwapBuffers(gl->mDeviceContext);
    }
  });
  builder.submitTask(std::move(task));
}
