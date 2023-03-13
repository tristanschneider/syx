#include "Precompile.h"
#include "Renderer.h"

#include "Queries.h"
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

      out vec2 oUV;

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

        gl_Position.x = pos.x;
        gl_Position.y = pos.y;
      }
      )";
    static constexpr const char* ps = R"(
        #version 330 core

        uniform sampler2D uTex;

        in vec2 oUV;
        layout(location = 0) out vec3 color;

        void main() {
          //uv flip hack to make the texture look right. uvs are bottom left to top right, picture is loaded top left to bottom right
          color = texture(uTex, vec2(oUV.x, 1.0-oUV.y)).rgb;
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
    result.uv = Shader::_createTextureSamplerUniform(quadShader, "uUV");
    result.worldToView = glGetUniformLocation(quadShader, "uWorldToView");
    result.texture = glGetUniformLocation(quadShader, "uTex");
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
  state.mQuadUniforms = _createQuadUniforms(state.mQuadShader);
  state.mDebug = _createDebugDrawer();
}

void _renderDebug(GameDatabase& db, RendererDatabase& renderDB) {
  OGLState& state = std::get<Row<OGLState>>(std::get<GraphicsContext>(renderDB.mTables).mRows).at(0);
  DebugDrawer& debug = state.mDebug;
  auto& lineTable = std::get<DebugLineTable>(db.mTables);
  auto& linesToDraw = std::get<Row<DebugPoint>>(lineTable.mRows);
  if(linesToDraw.size()) {
    auto e =  glGetError();
    glBindBuffer(GL_ARRAY_BUFFER, debug.mVBO);
    e =  glGetError();
    if(debug.mLastSize < linesToDraw.size()) {
      glBufferData(GL_ARRAY_BUFFER, sizeof(DebugPoint)*linesToDraw.size(), nullptr, GL_DYNAMIC_DRAW);
      e =  glGetError();
      debug.mLastSize = linesToDraw.size();
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(DebugPoint)*linesToDraw.size(), linesToDraw.mElements.data());
    e =  glGetError();
    glBindVertexArray(debug.mVAO);
    e =  glGetError();
    glUseProgram(debug.mShader);
    e =  glGetError();
    Queries::viewEachRow<Row<Camera>>(db, [&](Row<Camera>& cameras) {
      for(const Camera& camera : cameras.mElements) {
        glm::mat4 worldToView = glm::inverse(
          glm::translate(glm::vec3(camera.x, camera.y, 0.0f)) *
          glm::rotate(camera.angle, glm::vec3(0, 0, -1)) *
          glm::scale(glm::vec3(camera.zoom))
        );
        glUniformMatrix4fv(debug.mWVPUniform, 1, GL_FALSE, &worldToView[0][0]);

        e =  glGetError();
        glDrawArrays(GL_LINES, 0, linesToDraw.size());
        e =  glGetError();
      }
    });

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  TableOperations::resizeTable(lineTable, 0);
}

glm::mat4 _getWorldToView(const Camera& camera, float aspectRatio) {
  glm::vec3 scale = glm::vec3(camera.zoom);
  scale.x *= aspectRatio;
  return glm::inverse(
    glm::translate(glm::vec3(camera.x, camera.y, 0.0f)) *
    glm::rotate(camera.angle, glm::vec3(0, 0, -1)) *
    glm::scale(scale)
  );
}

void Renderer::render(GameDatabase& db, RendererDatabase& renderDB) {
  //TODO: separate step
  _processRequests(db, renderDB);

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
  /*
  glUseProgram(state.mQuadShader);
  Queries::viewEachRow<Row<Camera>>(db, [&](Row<Camera>& cameras) {
    for(const Camera& camera : cameras.mElements) {
      Queries::viewEachRow<FloatRow<Tags::Pos, Tags::X>,
        FloatRow<Tags::Pos, Tags::Y>,
        FloatRow<Tags::Rot, Tags::CosAngle>,
        FloatRow<Tags::Rot, Tags::SinAngle>,
        Row<CubeSprite>,
        SharedRow<TextureReference>>(db,
          [&](FloatRow<Tags::Pos, Tags::X>& posX,
            FloatRow<Tags::Pos, Tags::Y>& posY,
            FloatRow<Tags::Rot, Tags::CosAngle>& rotationX,
            FloatRow<Tags::Rot, Tags::SinAngle>& rotationY,
            Row<CubeSprite>& sprite,
            SharedRow<TextureReference>& texture) {

          size_t count = posX.size();
          if(!count) {
            return;
          }

          GLuint oglTexture = _getTextureByID(texture.at().mId, std::get<TexturesTable>(renderDB.mTables));
          if(!oglTexture) {
            return;
          }

          glBindBuffer(GL_TEXTURE_BUFFER, state.mQuadUniforms.posX.buffer);
          glBufferData(GL_TEXTURE_BUFFER, sizeof(float)*count, posX.mElements.data(), GL_STATIC_DRAW);
          glBindBuffer(GL_TEXTURE_BUFFER, state.mQuadUniforms.posY.buffer);
          glBufferData(GL_TEXTURE_BUFFER, sizeof(float)*count, posY.mElements.data(), GL_STATIC_DRAW);
          glBindBuffer(GL_TEXTURE_BUFFER, state.mQuadUniforms.rotX.buffer);
          glBufferData(GL_TEXTURE_BUFFER, sizeof(float)*count, rotationX.mElements.data(), GL_STATIC_DRAW);
          glBindBuffer(GL_TEXTURE_BUFFER, state.mQuadUniforms.rotY.buffer);
          glBufferData(GL_TEXTURE_BUFFER, sizeof(float)*count, rotationY.mElements.data(), GL_STATIC_DRAW);

          //TODO: doesn't change every frame
          glBindBuffer(GL_TEXTURE_BUFFER, state.mQuadUniforms.uv.buffer);
          glBufferData(GL_TEXTURE_BUFFER, sizeof(float)*count*4, sprite.mElements.data(), GL_STATIC_DRAW);

          glBindBuffer(GL_ARRAY_BUFFER, state.mQuadVertexBuffer);
          //Could tie these to a vao, but that would also require and index buffer all of which seems like overkill
          glEnableVertexAttribArray(0);
          //First attribute, 2 float elements that shouldn't be normalized, tightly packed, no offset
          glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

          const glm::mat4 worldToView = _getWorldToView(camera, aspectRatio);

          glUniformMatrix4fv(state.mQuadUniforms.worldToView, 1, GL_FALSE, &worldToView[0][0]);
          int textureIndex = 0;
          _bindTextureSamplerUniform(state.mQuadUniforms.posX, GL_R32F, textureIndex++);
          _bindTextureSamplerUniform(state.mQuadUniforms.posY, GL_R32F, textureIndex++);
          _bindTextureSamplerUniform(state.mQuadUniforms.rotX, GL_R32F, textureIndex++);
          _bindTextureSamplerUniform(state.mQuadUniforms.rotY, GL_R32F, textureIndex++);
          _bindTextureSamplerUniform(state.mQuadUniforms.uv, GL_RGBA32F, textureIndex++);
          glActiveTexture(GL_TEXTURE0 + textureIndex);
          glBindTexture(GL_TEXTURE_2D, oglTexture);
          glUniform1i(state.mQuadUniforms.texture, textureIndex++);

          glDrawArraysInstanced(GL_TRIANGLES, 0, 6, GLsizei(count));
      });
    }
  });
  */
  //_renderDebug(db, renderDB);

  Queries::viewEachRow<Row<Camera>>(db, [&](Row<Camera>& cameras) {
    for(const Camera& camera : cameras.mElements) {
      ParticleUniforms uniforms;
      uniforms.worldToView = _getWorldToView(camera, aspectRatio);

      //TODO: need all the buffer data not just the last one
      CubeSpriteInfo info;
      info.count = 1;
      info.posX = state.mQuadUniforms.posX;
      info.posY = state.mQuadUniforms.posY;
      info.quadVertexBuffer = state.mQuadVertexBuffer;
      info.rotX = state.mQuadUniforms.rotX;
      info.rotY = state.mQuadUniforms.rotY;
      //ParticleRenderer::renderNormals(data, uniforms, info);

      uniforms.worldToView = glm::identity<glm::mat4>();
      ParticleRenderer::update(data, uniforms, frameIndex);
      ParticleRenderer::render(data, uniforms, frameIndex);
    }
  });

 // Debug::pictureInPicture(debug, { 50, 50 }, { 350, 350 }, data.mSceneTexture);

  SwapBuffers(state.mDeviceContext);
}
