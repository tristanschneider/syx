#include "Precompile.h"
#include "Renderer.h"

#include "Queries.h"
#include "STBInterface.h"
#include "Table.h"

namespace {
  struct Mat3 {
    float* data() {
      return reinterpret_cast<float*>(this);
    }
    float a = 0; float b = 0; float c = 0;
    float d = 0; float e = 0; float f = 0;
    float g = 0; float h = 0; float i = 0;
  };

  struct QuadShader {
    static constexpr const char* vs = R"(
      #version 330 core
      layout(location = 0) in vec2 aPosition;

      uniform mat3 uWorldToView;
      uniform samplerBuffer uPosX;
      uniform samplerBuffer uPosY;
      uniform samplerBuffer uRot;
      uniform samplerBuffer uUV;

      out vec2 oUV;

      void main() {
        gl_Position = vec4(aPosition.xy*0.1, 0, 1.0);

        //TODO: should build transform on CPU to avoid per-vertex waste
        int i = gl_InstanceID;
        float angle = texelFetch(uRot, i).r;
        float cosAngle = cos(angle);
        float sinAngle = sin(angle);
        vec2 pos = aPosition;
        //2d rotation matrix multiply
        pos = vec2(pos.x*cosAngle - pos.y*sinAngle,
          pos.x*sinAngle + pos.y*cosAngle);
        pos += vec2(texelFetch(uPosX, i).r, texelFetch(uPosY, i).r);

        pos = (uWorldToView*vec3(pos, 1)).xy;

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

  void _getStatusWithInfo(GLuint handle, GLenum status, GLint& logLen, GLint& result) {
    result = GL_FALSE;
    glGetShaderiv(handle, status, &result);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
  }

  void _compileShader(GLuint shaderHandle, const char* source) {
    //Compile Shader
    glShaderSource(shaderHandle, 1, &source, NULL);
    glCompileShader(shaderHandle);

    GLint result, logLen;
    _getStatusWithInfo(shaderHandle, GL_COMPILE_STATUS, logLen, result);
    //Check Shader
    if(logLen > 0) {
      std::string error(logLen + 1, 0);
      glGetShaderInfoLog(shaderHandle, logLen, NULL, &error[0]);
      printf("%s\n", error.c_str());
    }
  }

  GLuint _loadShader(const char* vsSource, const char* psSource) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint ps = glCreateShader(GL_FRAGMENT_SHADER);
    _compileShader(vs, vsSource);
    _compileShader(ps, psSource);

    GLuint result{ glCreateProgram() };
    glAttachShader(result, vs);
    glAttachShader(result, ps);
    glLinkProgram(result);
    glValidateProgram(result);

    GLint glValidationStatus{};
    glGetProgramiv(result, GL_VALIDATE_STATUS, &glValidationStatus);
    if(glValidationStatus == GL_FALSE) {
      printf("Error linking shader\n");
      return 0;
    }

    //Once program is linked we can get rid of the individual shaders
    glDetachShader(result, vs);
    glDetachShader(result, ps);

    glDeleteShader(vs);
    glDeleteShader(ps);

    return result;
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

  TextureSamplerUniform _createTextureSamplerUniform(GLuint quadShader, const char* name) {
    TextureSamplerUniform result;
    result.uniformID = glGetUniformLocation(quadShader, name);
    glGenBuffers(1, &result.buffer);
    glGenTextures(1, &result.texture);
    return result;
  }

  QuadUniforms _createQuadUniforms(GLuint quadShader) {
    QuadUniforms result;
    result.posX = _createTextureSamplerUniform(quadShader, "uPosX");
    result.posY = _createTextureSamplerUniform(quadShader, "uPosY");
    result.rot = _createTextureSamplerUniform(quadShader, "uRot");
    result.uv = _createTextureSamplerUniform(quadShader, "uUV");
    result.worldToView = glGetUniformLocation(quadShader, "uWorldToView");
    result.texture = glGetUniformLocation(quadShader, "uTex");
    return result;
  }

  void _loadTexture(TextureLoadRequest& request, TexturesTable& textures) {
    ImageData data = STBInterface::loadImageFromFile(request.mFileName.c_str(), 3);
    if(!data.mBytes) {
      request.mStatus = RequestStatus::Failed;
      return;
    }

    GLuint textureHandle;
    glGenTextures(1, &textureHandle);
    glBindTexture(GL_TEXTURE_2D, textureHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GLsizei(data.mWidth), GLsizei(data.mHeight), 0, GL_RGB, GL_UNSIGNED_BYTE, data.mBytes);
    //Define sampling mode, no mip maps snap to nearest
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

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

void Renderer::initDeviceContext(GraphicsContext::ElementRef& context) {
  OGLState& state = context.get<0>();
  WindowData& window = context.get<1>();

  state.mDeviceContext = GetDC(window.mWindow);
  _initDevice(state.mDeviceContext, 32, 24, 8, 0);
  state.mGLContext = createGLContext(state.mDeviceContext);
  glewInit();

  const char* versionGL =  (char*)glGetString(GL_VERSION);
  printf("version %s", versionGL);

  state.mQuadShader = _loadShader(QuadShader::vs, QuadShader::ps);
  state.mQuadVertexBuffer = _createQuadBuffers();
  state.mQuadUniforms = _createQuadUniforms(state.mQuadShader);
}

void Renderer::render(GameDatabase& db, RendererDatabase& renderDB) {
  //TODO: separate step
  _processRequests(db, renderDB);

  db;
  OGLState& state = std::get<Row<OGLState>>(std::get<GraphicsContext>(renderDB.mTables).mRows).at(0);
  const WindowData& window = std::get<Row<WindowData>>(std::get<GraphicsContext>(renderDB.mTables).mRows).at(0);

  glViewport(0, 0, window.mWidth, window.mHeight);

  static float hack = 0.f;
  hack += 0.001f;
  if(hack > 1.0f) hack = 0;

  glClearColor(0.0f, 0.0f, hack, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  glUseProgram(state.mQuadShader);

  Queries::viewEachRow<FloatRow<Tags::Pos, Tags::X>,
    FloatRow<Tags::Pos, Tags::Y>,
    FloatRow<Tags::Rot, Tags::Angle>,
    Row<CubeSprite>,
    SharedRow<TextureReference>>(db,
      [&](FloatRow<Tags::Pos, Tags::X>& posX,
        FloatRow<Tags::Pos, Tags::Y>& posY,
        FloatRow<Tags::Rot, Tags::Angle>& rotation,
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
      glBindBuffer(GL_TEXTURE_BUFFER, state.mQuadUniforms.rot.buffer);
      glBufferData(GL_TEXTURE_BUFFER, sizeof(float)*count, rotation.mElements.data(), GL_STATIC_DRAW);
      //TODO: doesn't change every frame
      glBindBuffer(GL_TEXTURE_BUFFER, state.mQuadUniforms.uv.buffer);
      glBufferData(GL_TEXTURE_BUFFER, sizeof(float)*count*4, sprite.mElements.data(), GL_STATIC_DRAW);

      glBindBuffer(GL_ARRAY_BUFFER, state.mQuadVertexBuffer);
      //Could tie these to a vao, but that would also require and index buffer all of which seems like overkill
      glEnableVertexAttribArray(0);
      //First attribute, 2 float elements that shouldn't be normalized, tightly packed, no offset
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

      Mat3 worldToView;
      const float scale = 0.015f;
      const float camPosX = 0.0f;
      const float camPosY = 0.0f;
      worldToView.a = scale;                      ; worldToView.c = -camPosX;
      ;                    worldToView.e = scale; ; worldToView.f = -camPosY;
                                                    worldToView.i = 1.0f;

      glUniformMatrix3fv(state.mQuadUniforms.worldToView, 1, GL_FALSE, worldToView.data());
      _bindTextureSamplerUniform(state.mQuadUniforms.posX, GL_R32F, 0);
      _bindTextureSamplerUniform(state.mQuadUniforms.posY, GL_R32F, 1);
      _bindTextureSamplerUniform(state.mQuadUniforms.rot, GL_R32F, 2);
      _bindTextureSamplerUniform(state.mQuadUniforms.uv, GL_RGBA32F, 3);
      glActiveTexture(GL_TEXTURE0 + 4);
      glBindTexture(GL_TEXTURE_2D, oglTexture);
      glUniform1i(state.mQuadUniforms.texture, 4);

      glDrawArraysInstanced(GL_TRIANGLES, 0, 6, GLsizei(count));
  });

  SwapBuffers(state.mDeviceContext);
}
