#include "Precompile.h"
#include "ParticleRenderer.h"

#include "Debug.h"
#include "Quad.h"
#include "Shader.h"

namespace {
  const char* particleVS = R"(
    #version 330
    layout(location = 0) in vec2 mPosition;
    layout(location = 1) in vec2 mVelocity;
    layout(location = 2) in float mType;

    uniform sampler2D uSceneTex;

    out vec2 mPosition0;
    out vec2 mVelocity0;
    out float mType0;

    //Float precision is a bit wonky so snap it down so that zero velocity doesn't cause small drift
    float roundNearZero(float v) {
      int precisionI = 255;
      float precisionF = float(precisionI);
      int i = int(v*precisionF);
      return float(i)/precisionF;
    }

    void main() {
      //Texture position transform from NDC to texture coordinate space
      vec2 texCoord = vec2(mPosition.x*0.5f + 0.5f, mPosition.y*0.5f + 0.5f);
      //Fetch value and shift from range [0, 1] back to [-0.5, 0.5]
      vec4 sceneData = texture(uSceneTex, texCoord).rgba;
      vec2 collisionNormal = sceneData.rg - vec2(0.5f, 0.5f);
      //Velocity of object the particle is colliding with along normal
      float collidingVelocity = sceneData.b;
      collisionNormal = vec2(roundNearZero(collisionNormal.x), roundNearZero(collisionNormal.y));

      const float packID = 1.0f/255.0f;
      float myID = roundNearZero(float((gl_VertexID % 254) + 1)*packID);
      float collidingID = roundNearZero(sceneData.a);

      vec2 v = mVelocity;
      float particleVel = dot(collisionNormal, mVelocity);
      float impulse = max(0, collidingVelocity - particleVel);
      if(myID == collidingID) {
        impulse = 0.0f;
      }
      v += collisionNormal*impulse;

      v *= 0.995f;
      float dt = 0.016f;
      mPosition0 = mPosition + v*dt;
      mVelocity0 = v;
      mType0 = mType;
    }
  )";

  const char* particleRenderVS = R"(
    #version 330
    layout(location = 0) in vec2 mPosition;
    uniform mat4 uWorldToView;
    void main() { gl_Position = uWorldToView*vec4(mPosition, 0.0, 1.0); }
  )";

  const char* particleRenderPS = R"(
    #version 330 core
    out vec3 oColor;
    void main() { oColor = vec3(0.9, 0.9, 0.9); }
  )";

  const char* particleNormalRenderVS = R"(
    #version 330
    layout(location = 0) in vec2 mPosition;
    layout(location = 1) in vec2 mVelocity;

    out vec2 mVelocity1;
    //Vertex id is encoded in alpha channel to allow preventing self-collisions
    flat out float mID;

    void main() {
      gl_Position = vec4(mPosition, 0.0f, 1.0f);
      mVelocity1 = mVelocity;
      const float packID = 1.0f/255.0f;
      mID = float((gl_VertexID % 254) + 1)*packID;
    }
  )";

  const char* particleNormalRenderPS = R"(
    #version 330 core

    in vec2 mVelocity1;
    flat in float mID;
    out vec4 oColor;
    void main() {
      //Get position in rasterized point from top left origin and subtract center to get outward facing normal
      vec2 normal = gl_PointCoord - vec2(0.5f, 0.5f);
      //Flip from top left origin to bottom  left origin
      normal.y = -normal.y;

      float len = max(0.001f, length(normal));
      normal *= 1.0f/len;
      const float particleCollisionBias = 0.01f;
      float velocityAlongNormal = dot(normal, mVelocity1) + particleCollisionBias;
      //Shift normal from [-1,1] to [0, 1]
      vec2 packedNormal = normal*0.5f + vec2(0.5f, 0.5f);
      oColor = vec4(packedNormal, velocityAlongNormal, mID);
    }
  )";

  const char* quadNormalVS = R"(
    #version 330 core
    layout(location = 0) in vec2 aPosition;

    uniform mat4 uWorldToView;
    uniform samplerBuffer uPosX;
    uniform samplerBuffer uPosY;
    uniform samplerBuffer uRotX;
    uniform samplerBuffer uRotY;

    flat out int oInstanceID;
    out vec2 oUV;

    void main() {
        int i = gl_InstanceID;
        float cosAngle = texelFetch(uRotX, i).r;
        float sinAngle = texelFetch(uRotY, i).r;
        vec2 pos = aPosition;
        //2d rotation matrix multiply
        pos = vec2(pos.x*cosAngle - pos.y*sinAngle,
          pos.x*sinAngle + pos.y*cosAngle);
        pos += vec2(texelFetch(uPosX, i).r, texelFetch(uPosY, i).r);

        pos = (uWorldToView*vec4(pos, 0, 1)).xy;

        vec2 scale = vec2(0.5f, 0.5f);
        //Order of uUV data is uMin, vMin, uMax, vMax
        //Order of vertices in quad is:
        //1   5 4
        //2 0   3
        //Manually figure out which vertex this is to pick out the uv
        //An easier way would be duplicating the uv data per vertex
        vec2 uvMin = -scale;
        vec2 uvMax = scale;
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
        oInstanceID = i;
      }
  )";

  const char* quadNormalPS = R"(
      #version 330 core
      uniform samplerBuffer uRotX;
      uniform samplerBuffer uRotY;
      uniform samplerBuffer uVelX;
      uniform samplerBuffer uVelY;
      uniform samplerBuffer uVelA;

      flat in int oInstanceID;
      in vec2 oUV;
      layout(location = 0) out vec3 color;

      void main() {
        //Pick correct normal based on uv coordinates
        vec2 normal;
        float depth = 0.0f;
        if(abs(oUV.x) > abs(oUV.y)) {
          //X is the dominant coordinate, use it as the normal in whichever direction it is, length is up to 0.5
          normal = vec2(sign(oUV.x), 0.0f);
          depth = oUV.x;
        }
        else {
          normal = vec2(0.0f, sign(oUV.y));
          depth = oUV.y;
        }
        depth = 0.5 - abs(depth);

        //Rotate computed normal to world space then write it down as a color
        float cosAngle = texelFetch(uRotX, oInstanceID).r;
        float sinAngle = texelFetch(uRotY, oInstanceID).r;
        //2d rotation matrix multiply
        normal = vec2(normal.x*cosAngle - normal.y*sinAngle,
          normal.x*sinAngle + normal.y*cosAngle);

        vec2 linVel = vec2(texelFetch(uVelX, oInstanceID).r, texelFetch(uVelY, oInstanceID).r);
        float vA = texelFetch(uVelA, oInstanceID).r;
        //UV is vector from center to point, so the same rVector that can be used for angular velocity
        //cross product of [0,0,a]x[rx,ry,0]
        vec2 angularPortion = vec2(-oUV.y, oUV.x)*vA;
        vec2 velocityAtPoint = linVel + angularPortion;
        float velocityAlongNormal = dot(velocityAtPoint, normal);

        float contactBias = 0.01f;
        float velocityScalar = 40.0f;
        float outVelocity = contactBias*depth + velocityAlongNormal*velocityScalar;

        //shift from range [-0.5, 0.5] to [0, 1] since that's what colors get clamped to
        normal += vec2(0.5, 0.5);

        //Store normal as color in texture
        color = vec3(normal, outVelocity);
      }
  )";
}


namespace {
  struct TransformTargets {
    TransformTargets(size_t index)
      : src(index % 2)
      , dst((index + 1) % 2) {
    }
    size_t src = 0;
    size_t dst = 0;
  };

  void bindForUpdate(const ParticleData& data, const TransformTargets& targets, const ParticleUniforms& uniforms) {
    glUseProgram(data.mUpdateShader.program);

    glEnable(GL_RASTERIZER_DISCARD);

    glBindBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[targets.src]);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, data.mFeedbacks[targets.dst]);

    glBeginTransformFeedback(GL_POINTS);

    ParticleData::ParticleAttributes::bind();

    data.mSceneTexture;
    int textureIndex = 0;
    glActiveTexture(GL_TEXTURE0 + textureIndex);
    glBindTexture(GL_TEXTURE_2D, data.mSceneTexture);
    glUniform1i(data.mUpdateShader.sceneTexture, textureIndex++);

    uniforms;
    //glUniformMatrix4fv(data.mUpdateShader.worldToView, 1, GL_FALSE, &uniforms.worldToView[0][0]);

  }

  void unbindForUpdate() {
    glEndTransformFeedback();
    ParticleData::ParticleAttributes::unbind();

    //glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    //glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

    glDisable(GL_RASTERIZER_DISCARD);
    glUseProgram(0);
  }
}

void ParticleRenderer::init(ParticleData& data) {
  constexpr size_t particleCount = 100000;

  std::array<GLfloat, 2> range;
  glGetFloatv(GL_POINT_SIZE_RANGE, range.data());
  printf("Point size range %f to %f", range[0], range[1]);

  glGenTransformFeedbacks(data.mFeedbacks.size(), data.mFeedbacks.data());
  glGenBuffers(data.mFeedbackBuffers.size(), data.mFeedbackBuffers.data());

  std::vector<ParticleData::Particle> particles(particleCount);
  const int rowCols = static_cast<int>(std::sqrt(float(particleCount)));
  for(size_t i = 0; i < particles.size(); ++i) {
    const int r = i % rowCols;
    const int c = i / rowCols;
    particles[i].pos.x = float(r)/float(rowCols);
    particles[i].pos.y = float(c)/float(rowCols);
    particles[i].vel = glm::vec2(-0.1f);
  }

  for(size_t i = 0; i < data.mFeedbackBuffers.size(); ++i) {
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, data.mFeedbacks[i]);
    glBindBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[i]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ParticleData::Particle)*particles.size(), particles.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, data.mFeedbackBuffers[i]);
  }

  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  Shader::compileShader(vs, particleVS);

  data.mUpdateShader.program = glCreateProgram();
  glAttachShader(data.mUpdateShader.program, vs);
  //glAttachShader(data.mUpdateShader.program, gs);
  const GLchar* varyings[] = {
    "mPosition0",
    "mVelocity0",
    "mType0"
  };
  glTransformFeedbackVaryings(data.mUpdateShader.program, 3, varyings, GL_INTERLEAVED_ATTRIBS);

  if(!Shader::_link(data.mUpdateShader.program)) {
    printf("failed to created particle shader");
  }
  Shader::_detachAndDestroy(data.mUpdateShader.program, vs);
  data.mUpdateShader.sceneTexture = glGetUniformLocation(data.mUpdateShader.program, "uSceneTex");

  data.mRenderShader.program = Shader::loadShader(particleRenderVS, particleRenderPS);
  data.mRenderShader.worldToView = glGetUniformLocation(data.mRenderShader.program, "uWorldToView");

  data.mParticleSceneShader.mProgram = Shader::loadShader(particleNormalRenderVS, particleNormalRenderPS);

  //Populate initial feedback data
  ParticleUniforms uniforms;
  uniforms.worldToView = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);
  for(size_t i = 0; i < data.mFeedbackBuffers.size(); ++i) {
    TransformTargets target(i);

    bindForUpdate(data, TransformTargets(i), uniforms);
    glDrawArrays(GL_POINTS, 0, particles.size());
    unbindForUpdate();
  }

  glGenFramebuffers(1, &data.mSceneFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, data.mSceneFBO);
  glGenTextures(1, &data.mSceneTexture);
  glBindTexture(GL_TEXTURE_2D, data.mSceneTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ParticleData::SCENE_WIDTH, ParticleData::SCENE_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, data.mSceneTexture, 0);

  GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
  glDrawBuffers(1, &drawBuffer);

  constexpr float zero = 0.5f;
  constexpr float unused = 0.0f;
  constexpr float edgeBias = 0.5f;
  constexpr float borderForce = 0.5f;
  glClearColor(zero, zero, unused, unused);
  glClear(GL_COLOR_BUFFER_BIT);

  glEnable(GL_SCISSOR_TEST);
  //Put a one pixel border into the texture on the edges pointing back towards the middle
  //Left edge
  glScissor(0, 0, 1, ParticleData::SCENE_HEIGHT);
  glClearColor(zero + edgeBias, zero, borderForce, unused);
  glClear(GL_COLOR_BUFFER_BIT);
  //Right edge
  glScissor(ParticleData::SCENE_WIDTH - 1, 0, 1, ParticleData::SCENE_HEIGHT);
  glClearColor(zero - edgeBias, zero, borderForce, unused);
  glClear(GL_COLOR_BUFFER_BIT);
  //Top edge
  glScissor(0, ParticleData::SCENE_HEIGHT - 1, ParticleData::SCENE_WIDTH, 1);
  glClearColor(zero, zero - edgeBias, borderForce, unused);
  glClear(GL_COLOR_BUFFER_BIT);
  //Bottom edge
  glScissor(0, 0, ParticleData::SCENE_WIDTH, 1);
  glClearColor(zero, zero + edgeBias, borderForce, unused);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST);

  data.mSceneShader.mProgram = Shader::loadShader(quadNormalVS, quadNormalPS);
  data.mSceneShader.posX = glGetUniformLocation(data.mSceneShader.mProgram, "uPosX");
  data.mSceneShader.posY = glGetUniformLocation(data.mSceneShader.mProgram, "uPosY");
  data.mSceneShader.rotX = glGetUniformLocation(data.mSceneShader.mProgram, "uRotX");
  data.mSceneShader.rotY = glGetUniformLocation(data.mSceneShader.mProgram, "uRotY");
  data.mSceneShader.velX = glGetUniformLocation(data.mSceneShader.mProgram, "uVelX");
  data.mSceneShader.velY = glGetUniformLocation(data.mSceneShader.mProgram, "uVelY");
  data.mSceneShader.angVel = glGetUniformLocation(data.mSceneShader.mProgram, "uVelA");
  data.mSceneShader.worldToView = glGetUniformLocation(data.mSceneShader.mProgram, "uWorldToView");

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ParticleRenderer::update(const ParticleData& data, const ParticleUniforms& uniforms, size_t frameIndex) {
  const TransformTargets target(frameIndex);
  bindForUpdate(data, target, uniforms);
  glDrawTransformFeedback(GL_POINTS, data.mFeedbacks[target.src]);
  unbindForUpdate();
}

namespace {
  void _bindTextureSamplerUniform(const TextureSamplerUniform& sampler, GLenum format, int index, GLuint uniform) {
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_BUFFER, sampler.texture);
    glTexBuffer(GL_TEXTURE_BUFFER, format, sampler.buffer);
    glUniform1i(uniform, index);
  }
}

void ParticleRenderer::renderNormalsBegin(const ParticleData& data) {
  glViewport(0, 0, ParticleData::SCENE_WIDTH, ParticleData::SCENE_HEIGHT);
  //Leave the one pixel border to make particles go back into the scene
  glEnable(GL_SCISSOR_TEST);
  glScissor(1, 1, ParticleData::SCENE_WIDTH - 2, ParticleData::SCENE_HEIGHT - 2);
  glBindFramebuffer(GL_FRAMEBUFFER, data.mSceneFBO);
  //Normals are stored in color in the range [0, 1] but shifted and used as [-0.5, 0.5], meaning that 0.5 raw color has the meaning of 0
  glClearColor(0.5f, 0.5f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void ParticleRenderer::renderQuadNormalsBegin(const ParticleData& data) {
  glUseProgram(data.mSceneShader.mProgram);
}

void ParticleRenderer::renderNormalsEnd() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_SCISSOR_TEST);
}

void ParticleRenderer::renderQuadNormalsEnd() {
  glUseProgram(0);
}


void ParticleRenderer::renderNormals(const ParticleData& data, const ParticleUniforms& uniforms, const CubeSpriteInfo& sprites) {
  glBindBuffer(GL_ARRAY_BUFFER, sprites.quadVertexBuffer);
  QuadVertexAttributes::bind();
  glUniformMatrix4fv(data.mSceneShader.worldToView, 1, GL_FALSE, &uniforms.worldToParticle[0][0]);
  int textureIndex = 0;
  _bindTextureSamplerUniform(sprites.posX, GL_R32F, textureIndex++, data.mSceneShader.posX);
  _bindTextureSamplerUniform(sprites.posY, GL_R32F, textureIndex++, data.mSceneShader.posY);
  _bindTextureSamplerUniform(sprites.rotX, GL_R32F, textureIndex++, data.mSceneShader.rotX);
  _bindTextureSamplerUniform(sprites.rotY, GL_R32F, textureIndex++, data.mSceneShader.rotY);
  _bindTextureSamplerUniform(sprites.velX, GL_R32F, textureIndex++, data.mSceneShader.velX);
  _bindTextureSamplerUniform(sprites.velY, GL_R32F, textureIndex++, data.mSceneShader.velY);
  _bindTextureSamplerUniform(sprites.velA, GL_R32F, textureIndex++, data.mSceneShader.angVel);
  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, GLsizei(sprites.count));
}

void ParticleRenderer::renderParticleNormals(const ParticleData& data, const ParticleUniforms& uniforms, size_t frameIndex) {
  frameIndex;uniforms;
  const TransformTargets target(0);

  //Required for gl_PointCoord to be nonzero
  glEnable(GL_POINT_SPRITE);
  glPointSize(3.0f);
  glUseProgram(data.mParticleSceneShader.mProgram);
  glBindFramebuffer(GL_FRAMEBUFFER, data.mSceneFBO);
  glBindBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[target.dst]);

  ParticleData::ParticleAttributes::bind();

  glDrawTransformFeedback(GL_POINTS, data.mFeedbacks[target.dst]);

  ParticleData::ParticleAttributes::unbind();
  glUseProgram(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisable(GL_POINT_SPRITE);
}

void ParticleRenderer::render(const ParticleData& data, const ParticleUniforms& uniforms, size_t frameIndex) {
  frameIndex;uniforms;
  const TransformTargets target(frameIndex);

  glPointSize(2.0f);
  glUseProgram(data.mRenderShader.program);
  glBindBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[target.dst]);

  //Bind only position
  ParticleData::ParticleAttributes::bindRange(0, 1);

  glm::mat4 particleToView = uniforms.worldToView * uniforms.particleToWorld;
  glUniformMatrix4fv(data.mRenderShader.worldToView, 1, GL_FALSE, &particleToView[0][0]);
  glDrawTransformFeedback(GL_POINTS, data.mFeedbacks[target.dst]);

  ParticleData::ParticleAttributes::unbind();
  glUseProgram(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}
