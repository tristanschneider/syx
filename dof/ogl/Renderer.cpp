#include "Precompile.h"
#include "Renderer.h"

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
      uniform samplerBuffer uRotX;
      uniform samplerBuffer uRotY;
      uniform samplerBuffer uUV;
      uniform samplerBuffer uTint;

      out vec2 oUV;
      out vec4 oTint;

      void main() {
        gl_Position = vec4(aPosition.xy*0.1, 0, 1.0);

        //TODO: should build transform on CPU to avoid per-vertex waste
        int i = gl_InstanceID;
        float cosAngle = texelFetch(uRotX, i).r;
        float sinAngle = texelFetch(uRotY, i).r;
        vec2 pos = aPosition;
        //2d rotation matrix multiply
        pos = vec2(pos.x*cosAngle - pos.y*sinAngle,
          pos.x*sinAngle + pos.y*cosAngle);
        pos += vec2(texelFetch(uPosX, i).r, texelFetch(uPosY, i).r);

        pos = (uWorldToView*vec4(pos, 0, 1)).xy;

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

        gl_Position.x = pos.x;
        gl_Position.y = pos.y;
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
    glm::vec3 scale = glm::vec3(camera.camera.zoom);
    scale.x *= aspectRatio;
    return glm::inverse(
      glm::translate(glm::vec3(camera.pos.x, camera.pos.y, 0.0f)) *
      glm::rotate(camera.camera.angle, glm::vec3(0, 0, -1)) *
      glm::scale(scale)
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

  DebugDrawer _createDebugDrawer() {
    DebugDrawer drawer;
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

  void _loadTexture(TextureLoadRequest& request, TexturesTable& textures) {
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

    TexturesTable::ElementRef element = TableOperations::addToTable(textures);
    element.get<0>().mHandle = textureHandle;
    element.get<1>().mID = request.mImageID;

    request.mStatus = RequestStatus::Succeeded;

    STBInterface::deallocate(std::move(data));
  }

  void _processRequests(GameDatabase& db, RendererDatabase& renderDB) {
    Queries::viewEachRow<Row<TextureLoadRequest>>(db, [&](Row<TextureLoadRequest>& row) {
      for(TextureLoadRequest& req : row.mElements) {
        if(req.mStatus == RequestStatus::InProgress) {
          _loadTexture(req, std::get<TexturesTable>(renderDB.mTables));
        }
      }
    });
  }

  GLuint _getTextureByID(size_t id, const TexturesTable& textures) {
    auto& handles = std::get<Row<TextureGameHandle>>(textures.mRows).mElements;
    for(size_t i = 0; i < handles.size(); ++i) {
      if(handles[i].mID == id) {
        return std::get<Row<TextureGLHandle>>(textures.mRows).at(i).mHandle;
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

void Renderer::initDeviceContext(GraphicsContext::ElementRef& context) {
  OGLState& state = context.get<0>();
  WindowData& window = context.get<1>();

  state.mDeviceContext = GetDC(window.mWindow);
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
}

void Renderer::initGame(GameDatabase& db, RendererDatabase& renderDB) {
  OGLState& state = std::get<Row<OGLState>>(std::get<GraphicsContext>(renderDB.mTables).mRows).at(0);
  //Resize containers upon the first call, they never change
  RendererGlobalsAdapter globals = RendererTableAdapters::getGlobals({ renderDB });
  assert(globals.size);

  auto& quadPasses = globals.state->mQuadPasses;
  quadPasses.clear();
  Queries::viewEachRowWithTableID<Row<CubeSprite>>(db,
      [&](GameDatabase::ElementID id, Row<CubeSprite>&) {
        quadPasses.push_back({});
        auto pass = RendererTableAdapters::getQuadPass(quadPasses.back());
        pass.isImmobile->at(0) = Queries::getRowInTable<IsImmobile>(db, id) != nullptr;
        pass.pass->at(0).mQuadUniforms = _createQuadUniforms(state.mQuadShader);
  });
}

void _renderDebug(RendererDatabase& renderDB, float aspectRatio) {
  PROFILE_SCOPE("renderer", "debug");
  OGLState& state = std::get<Row<OGLState>>(std::get<GraphicsContext>(renderDB.mTables).mRows).at(0);
  DebugDrawer& debug = state.mDebug;
  auto renderDebug = RendererTableAdapters::getDebug({ renderDB });
  auto& linesToDraw = *renderDebug.points;
  if(linesToDraw.size()) {
    glBindBuffer(GL_ARRAY_BUFFER, debug.mVBO);
    if(debug.mLastSize < linesToDraw.size()) {
      glBufferData(GL_ARRAY_BUFFER, sizeof(DebugPoint)*linesToDraw.size(), nullptr, GL_DYNAMIC_DRAW);
      debug.mLastSize = linesToDraw.size();
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(DebugPoint)*linesToDraw.size(), linesToDraw.mElements.data());
    glBindVertexArray(debug.mVAO);
    glUseProgram(debug.mShader);
    for(const auto& renderCamera : state.mCameras) {
      glm::mat4 worldToView = _getWorldToView(renderCamera, aspectRatio);
      glUniformMatrix4fv(debug.mWVPUniform, 1, GL_FALSE, &worldToView[0][0]);

      glDrawArrays(GL_LINES, 0, linesToDraw.size());
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
}

template<class SrcRow, class DstRow>
std::shared_ptr<TaskNode> copyDataTask(const SrcRow& src, DstRow& dst) {
  static_assert(sizeof(src.at(0)) == sizeof(dst.at(0)));
  PROFILE_SCOPE("renderer", "extract row");
  return TaskNode::create([&src, &dst](...) {
    assert(src.size() == dst.size());
    constexpr size_t size = sizeof(src.at(0));
    std::memcpy(dst.mElements.data(), src.mElements.data(), dst.size()*size);
  });
}

TaskRange Renderer::extractRenderables(const GameDatabase& db, RendererDatabase& renderDB) {

  auto root = TaskNode::create([](...){});

  RendererGlobalsAdapter globals = RendererTableAdapters::getGlobals({ renderDB });
  assert(globals.size);

  auto& quadPasses = globals.state->mQuadPasses;

  size_t passIndex = 0;
  //Quads
  Queries::viewEachRowWithTableID<FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    FloatRow<Tags::Rot, Tags::CosAngle>,
    FloatRow<Tags::Rot, Tags::SinAngle>,
    Row<CubeSprite>,
    SharedRow<TextureReference>>(db,
      [&](GameDatabase::ElementID id,
        const FloatRow<Tags::Pos, Tags::X>& posX,
        const FloatRow<Tags::Pos, Tags::Y>& posY,
        const FloatRow<Tags::Rot, Tags::CosAngle>& rotationX,
        const FloatRow<Tags::Rot, Tags::SinAngle>& rotationY,
        const Row<CubeSprite>& sprite,
        const SharedRow<TextureReference>& texture) {

      QuadPassTable::Type& passTable = quadPasses[passIndex++];
      QuadPassAdapter pass = RendererTableAdapters::getQuadPass(passTable);
      //First resize the table
      auto passRoot = TaskNode::create([&passTable, &posX](...) {
         TableOperations::resizeTable(passTable, posX.size());
      });
      root->mChildren.push_back(passRoot);
      //Then copy each row of data to it
      passRoot->mChildren.push_back(copyDataTask(posX, *pass.posX));
      passRoot->mChildren.push_back(copyDataTask(posY, *pass.posY));
      passRoot->mChildren.push_back(copyDataTask(rotationX, *pass.rotX));
      passRoot->mChildren.push_back(copyDataTask(rotationY, *pass.rotY));
      passRoot->mChildren.push_back(copyDataTask(sprite, *pass.uvs));
      passRoot->mChildren.push_back(TaskNode::create([pass, &texture](...) {
        pass.texture->at(0) = texture.at(0).mId;
      }));
      const auto velX = Queries::getRowInTable<FloatRow<Tags::LinVel, Tags::X>>(db, id);
      const auto velY = Queries::getRowInTable<FloatRow<Tags::LinVel, Tags::Y>>(db, id);
      const auto velA = Queries::getRowInTable<FloatRow<Tags::AngVel, Tags::Angle>>(db, id);
      if(velX && velY && velA) {
        passRoot->mChildren.push_back(copyDataTask(*velX, *pass.linVelX));
        passRoot->mChildren.push_back(copyDataTask(*velY, *pass.linVelY));
        passRoot->mChildren.push_back(copyDataTask(*velA, *pass.angVel));
      }
      if(const auto tint = Queries::getRowInTable<Tint>(db, id)) {
        passRoot->mChildren.push_back(copyDataTask(*tint, *pass.tint));
      }
  });

  //Debug lines
  auto debug = RendererTableAdapters::getDebug({ renderDB });
  const auto& lineTable = std::get<DebugLineTable>(db.mTables);
  const auto& linesToDraw = std::get<Row<DebugPoint>>(lineTable.mRows);
  auto debugRoot = TaskNode::create([table{debug.table}, &linesToDraw](...) {
    TableOperations::resizeTable(*table, linesToDraw.size());
  });
  root->mChildren.push_back(debugRoot);
  debugRoot->mChildren.push_back(copyDataTask(linesToDraw, *debug.points));

  //Cameras
  auto& renderCameras = globals.state->mCameras;
  root->mChildren.push_back(TaskNode::create([&renderCameras, &db](...) {
    //Lazy since presumably there's just one
    renderCameras.clear();
    Queries::viewEachRow(db, [&](
        const Row<Camera>& cameras,
        const FloatRow<Tags::Pos, Tags::X>& posX,
        const FloatRow<Tags::Pos, Tags::Y>& posY
      ) {
        for(size_t i = 0; i < cameras.size(); ++i) {
          renderCameras.push_back({ glm::vec2{ posX.at(i), posY.at(i) }, cameras.at(i) });
        }
    });
  }));

  //Globals
  root->mChildren.push_back(TaskNode::create([&db, globals](...) {
    auto& g = std::get<GlobalGameData>(db.mTables);
    globals.state->mSceneState = std::get<SharedRow<SceneState>>(g.mRows).at();
  }));

  return TaskBuilder::buildDependencies(root);
}

TaskRange Renderer::clearRenderRequests(GameDatabase& db) {
  auto root = TaskNode::create([&db](...) {
    auto& lineTable = std::get<DebugLineTable>(db.mTables);
    TableOperations::resizeTable(lineTable, 0);
  });
  return TaskBuilder::buildDependencies(root);
}

void Renderer::processRequests(GameDatabase& db, RendererDatabase& renderDB) {
  _processRequests(db, renderDB);
}

void Renderer::render(RendererDatabase& renderDB) {
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

  OGLState& state = std::get<Row<OGLState>>(std::get<GraphicsContext>(renderDB.mTables).mRows).at(0);
  const WindowData& window = std::get<Row<WindowData>>(std::get<GraphicsContext>(renderDB.mTables).mRows).at(0);

  glViewport(0, 0, window.mWidth, window.mHeight);
  const float aspectRatio = window.mHeight ? float(window.mWidth)/float(window.mHeight) : 1.0f;

  glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  auto& cameras = state.mCameras;
  for(const auto& renderCamera : cameras) {
    PROFILE_SCOPE("renderer", "particles");

    ParticleUniforms uniforms;
    uniforms.worldToView = _getWorldToView(renderCamera, aspectRatio);
    const SceneState& scene = state.mSceneState;
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
    for(auto& passTable : state.mQuadPasses) {
      QuadPass& pass = RendererTableAdapters::getQuadPass(passTable).pass->at();
      //Hack to use the last count since it renders before the buffers are updated by quad drawing.
      if(pass.mLastCount) {
        CubeSpriteInfo info;
        info.count = pass.mLastCount;
        info.posX = pass.mQuadUniforms.posX;
        info.posY = pass.mQuadUniforms.posY;
        info.quadVertexBuffer = state.mQuadVertexBuffer;
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
    glViewport(0, 0, window.mWidth, window.mHeight);
    ParticleRenderer::render(data, uniforms, frameIndex);
  }

  glUseProgram(state.mQuadShader);
  for(const auto& renderCamera : cameras) {
    PROFILE_SCOPE("renderer", "geometry");
    for(auto& passTable : state.mQuadPasses) {
      auto passAdapter = RendererTableAdapters::getQuadPass(passTable);
      auto& posX = *passAdapter.posX;
      auto& posY = *passAdapter.posY;
      auto& rotationX = *passAdapter.rotX;
      auto& rotationY = *passAdapter.rotY;
      auto& sprite = *passAdapter.uvs;
      auto& texture = *passAdapter.texture;
      auto& velX = *passAdapter.linVelX;
      auto& velY = *passAdapter.linVelY;
      auto& velA = *passAdapter.angVel;
      auto& tint = *passAdapter.tint;
      auto& pass = passAdapter.pass->at();

      size_t count = posX.size();
      pass.mLastCount = count;
      if(!count) {
        continue;
      }

      GLuint oglTexture = _getTextureByID(texture.at(), std::get<TexturesTable>(renderDB.mTables));
      if(!oglTexture) {
        continue;
      }

      const size_t floatSize = sizeof(float)*count;
      glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.posX.buffer);
      glBufferData(GL_TEXTURE_BUFFER, floatSize, posX.mElements.data(), GL_STATIC_DRAW);
      glBindBuffer(GL_TEXTURE_BUFFER, pass.mQuadUniforms.posY.buffer);
      glBufferData(GL_TEXTURE_BUFFER, floatSize, posY.mElements.data(), GL_STATIC_DRAW);
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

      glBindBuffer(GL_ARRAY_BUFFER, state.mQuadVertexBuffer);
      //Could tie these to a vao, but that would also require and index buffer all of which seems like overkill
      QuadVertexAttributes::bind();

      const glm::mat4 worldToView = _getWorldToView(renderCamera, aspectRatio);

      glUniformMatrix4fv(pass.mQuadUniforms.worldToView, 1, GL_FALSE, &worldToView[0][0]);
      int textureIndex = 0;
      _bindTextureSamplerUniform(pass.mQuadUniforms.posX, GL_R32F, textureIndex++);
      _bindTextureSamplerUniform(pass.mQuadUniforms.posY, GL_R32F, textureIndex++);
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
  _renderDebug(renderDB, aspectRatio);

  //Debug::pictureInPicture(debug, { 50, 50 }, { 350, 350 }, data.mSceneTexture);
  glGetError();
}

void Renderer::swapBuffers(RendererDatabase& renderDB) {
  PROFILE_SCOPE("renderer", "swap");
  OGLState& state = std::get<Row<OGLState>>(std::get<GraphicsContext>(renderDB.mTables).mRows).at(0);
  SwapBuffers(state.mDeviceContext);
}
