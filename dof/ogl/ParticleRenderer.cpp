#include "Precompile.h"
#include "ParticleRenderer.h"

#include "Debug.h"
#include "Shader.h"

GLenum e{};

namespace {
  const char* particleVS = R"(
    #version 330
    layout(location = 0) in vec2 mPosition;
    layout(location = 1) in vec2 mVelocity;
    layout(location = 2) in float mType;

    //uniform mat4 uWorldToView;

    out vec2 mPosition0;
    out vec2 mVelocity0;
    out float mType0;

    void main() {
      //mPosition0 = vec2((uWorldToView*vec4(mPosition + mVelocity, 0.0, 1.0)).xy);
      //mPosition0.x = clamp(mPosition0.x, 0.0f, 1.0f);
      //mPosition0.y = clamp(mPosition0.y, 0.0f, 1.0f);

      //mPosition0.x = 0.15f;
      //mPosition0.y = 0.15f;

      mPosition0 = mPosition + mVelocity;
      mVelocity0 = mVelocity;
      mType0 = mType;
    }
  )";

  const char* particleGS = R"(
    #version 330
    layout(points) in;
    layout(points) out;
    layout(max_vertices = 100000) out;

    in vec2 mPosition0[];
    in vec2 mVelocity0[];
    in float mType0[];

    out vec2 mPosition1;
    out vec2 mVelocity1;
    out float mType1;

    void main() {
      mPosition1 = mPosition0[0];
      mVelocity1 = mVelocity0[0];
      mType1 = mType0[0];
      if(mType0[0] == 1.0) {
        EmitVertex();
        EndPrimitive();
      }
    }
  )";

  const char* particleRenderVS = R"(
    #version 330
    layout(location = 0) in vec2 mPosition;
    //uniform mat4 uWorldToView;
    //void main() { gl_Position = uWorldToView*vec4(mPosition, 0.0, 1.0); }
    void main() { gl_Position = vec4(mPosition, 0.0, 1.0); }
  )";

  const char* particleRenderPS = R"(
    #version 330 core
    out vec3 oColor;
    void main() { oColor = vec3(1, 0, 0); }
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

        //Order of uUV data is uMin, vMin, uMax, vMax
        //Order of vertices in quad is:
        //1   5 4
        //2 0   3
        //Manually figure out which vertex this is to pick out the uv
        //An easier way would be duplicating the uv data per vertex
        vec2 uvMin = vec2(-0.5f, -0.5f);
        vec2 uvMax = vec2(0.5f, 0.5f);
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

      flat in int oInstanceID;
      in vec2 oUV;
      layout(location = 0) out vec3 color;

      void main() {
        //Pick correct normal based on uv coordinates
        if(abs(oUV.x) > abs(oUV.y)) {
          //X is the dominant coordinate, use it as the normal in whichever direction it is, extended to length 1 at the max extent
          color = vec3(oUV.x*2.0f, 0.0f, 0.0f);
        }
        else {
          color = vec3(0.0f, oUV.y*2.0f, 0.0f);
        }

        //Rotate computed normal to world space then write it down as a color
        float cosAngle = texelFetch(uRotX, oInstanceID).r;
        float sinAngle = texelFetch(uRotY, oInstanceID).r;
        //2d rotation matrix multiply
        color = vec3(color.x*cosAngle - color.y*sinAngle,
          color.x*sinAngle + color.y*cosAngle, 0.0f);
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
  e = glGetError();

    //glEnable(GL_RASTERIZER_DISCARD);
  e = glGetError();

    glBindBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[targets.src]);
  e = glGetError();

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, data.mFeedbacks[targets.dst]);
    //glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, data.mFeedbacks[targets.dst]);
    //Should only be necessary upon intial creation of feedback buffer. NVidia bug?
    //glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, data.mFeedbackBuffers[targets.src]);

    
    
    e = glGetError();

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleData::Particle), (const void*)0);
  e = glGetError();

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleData::Particle), (const void*)(sizeof(float)*2));
  e = glGetError();

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleData::Particle), (const void*)(sizeof(float)*4));
  e = glGetError();

    uniforms;
    //glUniformMatrix4fv(data.mUpdateShader.worldToView, 1, GL_FALSE, &uniforms.worldToView[0][0]);

    glBeginTransformFeedback(GL_POINTS);
  e = glGetError();

  }

  void unbindForUpdate() {
    glEndTransformFeedback();
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    //glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    //glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

    glDisable(GL_RASTERIZER_DISCARD);
    glUseProgram(0);
  }
}

void ParticleRenderer::init(ParticleData& data) {
  e = glGetError();
  constexpr size_t particleCount = 10000;

  glGenTransformFeedbacks(data.mFeedbacks.size(), data.mFeedbacks.data());
  glGenBuffers(data.mFeedbackBuffers.size(), data.mFeedbackBuffers.data());

  std::vector<ParticleData::Particle> particles(particleCount);
  for(size_t i = 0; i < particles.size(); ++i) {
    particles[i].pos.x = float(i)*0.001f;
    particles[i].pos.y = float(i)*0.001f;
    particles[i].vel.y = (i % 2) ? 0.01f : -0.01f;
  }
  for(size_t i = 0; i < data.mFeedbackBuffers.size(); ++i) {
    std::vector<uint8_t> a,b;
    Debug::readBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, data.mFeedbacks[i], a);
    Debug::readBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[i], b);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, data.mFeedbacks[i]);

    Debug::readBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, data.mFeedbacks[i], a);
    Debug::readBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[i], b);

    glBindBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[i]);
    //glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, data.mFeedbackBuffers[i]);

    particles[0].type = 1;
    glBufferData(GL_ARRAY_BUFFER, sizeof(ParticleData::Particle)*particles.size(), particles.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, data.mFeedbackBuffers[i]);

    //glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    //glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

    Debug::readBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, data.mFeedbacks[i], a);
    Debug::readBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[i], b);
    ParticleData::Particle* pa = (ParticleData::Particle*)b.data();
    pa;

  }

  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  GLuint gs = glCreateShader(GL_GEOMETRY_SHADER);
  Shader::compileShader(vs, particleVS);
  Shader::compileShader(gs, particleGS);

  data.mUpdateShader.program = glCreateProgram();
  glAttachShader(data.mUpdateShader.program, vs);
  glAttachShader(data.mUpdateShader.program, gs);
  const GLchar* varyings[] = {
    "mPosition1",
    "mVelocity1",
    "mType1"
  };
  glTransformFeedbackVaryings(data.mUpdateShader.program, 3, varyings, GL_INTERLEAVED_ATTRIBS);
  e = glGetError();
  e = glGetError();

  if(!Shader::_linkAndValidate(data.mUpdateShader.program)) {
    printf("failed to created particle shader");
  }
  Shader::_detachAndDestroy(data.mUpdateShader.program, vs, gs);
  //data.mUpdateShader.worldToView = glGetUniformLocation(data.mUpdateShader.program, "uWorldToView");

  data.mRenderShader.program = Shader::loadShader(particleRenderVS, particleRenderPS);
  //data.mRenderShader.worldToView = glGetUniformLocation(data.mRenderShader.program, "uWorldToView");

  //Populate initial feedback data
  ParticleUniforms uniforms;
  uniforms.worldToView = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);
  for(size_t i = 0; i < data.mFeedbackBuffers.size(); ++i) {
    std::vector<uint8_t> a,b,c,d;
    TransformTargets target(0);
    Debug::readBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, data.mFeedbacks[target.src], a);
    Debug::readBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, data.mFeedbacks[target.dst], b);
    Debug::readBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[target.src], c);
    Debug::readBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[target.dst], d);

    bindForUpdate(data, TransformTargets(0), uniforms);
    e = glGetError();
    glDrawArrays(GL_POINTS, 0, particles.size());
    e = glGetError();
    unbindForUpdate();
    e = glGetError();

    Debug::readBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, data.mFeedbacks[target.src], a);
    Debug::readBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, data.mFeedbacks[target.dst], b);
    Debug::readBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[target.src], c);
    Debug::readBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[target.dst], d);

  }
  update(data, uniforms, 0);

  glGenFramebuffers(1, &data.mSceneFBO);
  e = glGetError();

  glBindFramebuffer(GL_FRAMEBUFFER, data.mSceneFBO);
  e = glGetError();

  glGenTextures(1, &data.mSceneTexture);
  e = glGetError();

  glBindTexture(GL_TEXTURE_2D, data.mSceneTexture);
  e = glGetError();

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ParticleData::SCENE_WIDTH, ParticleData::SCENE_HEIGHT, 0, GL_RGB, GL_FLOAT, nullptr);
  e = glGetError();

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  e = glGetError();

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  e = glGetError();

  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, data.mSceneTexture, 0);
  e = glGetError();

  GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
  glDrawBuffers(1, &drawBuffer);
  e = glGetError();

  data.mSceneShader.mProgram = Shader::loadShader(quadNormalVS, quadNormalPS);
  e = glGetError();

  data.mSceneShader.posX = glGetUniformLocation(data.mSceneShader.mProgram, "uPosX");
  e = glGetError();

  data.mSceneShader.posY = glGetUniformLocation(data.mSceneShader.mProgram, "uPosY");
  e = glGetError();

  data.mSceneShader.rotX = glGetUniformLocation(data.mSceneShader.mProgram, "uRotX");
  e = glGetError();

  data.mSceneShader.rotY = glGetUniformLocation(data.mSceneShader.mProgram, "uRotY");
  e = glGetError();

  data.mSceneShader.worldToView = glGetUniformLocation(data.mSceneShader.mProgram, "uWorldToView");
  e = glGetError();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

void ParticleRenderer::update(const ParticleData& data, const ParticleUniforms& uniforms, size_t frameIndex) {
  e = glGetError();
  const TransformTargets target(frameIndex);

  bindForUpdate(data, target, uniforms);
  e = glGetError();
  glDrawTransformFeedback(GL_POINTS, data.mFeedbacks[target.src]);
  e = glGetError();
  unbindForUpdate();
  e = glGetError();

  std::vector<uint8_t> b;
  Debug::readBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[1], b);
  ParticleData::Particle* pa = (ParticleData::Particle*)b.data();
  pa;
  printf("%f %f\n", pa->pos.x, pa->pos.y);


}

namespace {
  void _bindTextureSamplerUniform(const TextureSamplerUniform& sampler, GLenum format, int index, GLuint uniform) {
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_BUFFER, sampler.texture);
    glTexBuffer(GL_TEXTURE_BUFFER, format, sampler.buffer);
    glUniform1i(uniform, index);
  }
}

void ParticleRenderer::renderNormals(const ParticleData& data, const ParticleUniforms& uniforms, const CubeSpriteInfo& sprites) {
  data;sprites;uniforms;
  e = glGetError();

  glUseProgram(data.mSceneShader.mProgram);
  e = glGetError();

  glBindFramebuffer(GL_FRAMEBUFFER, data.mSceneFBO);
  e = glGetError();

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  e = glGetError();

  glBindBuffer(GL_ARRAY_BUFFER, sprites.quadVertexBuffer);
  e = glGetError();

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  e = glGetError();

  glUniformMatrix4fv(data.mSceneShader.worldToView, 1, GL_FALSE, &uniforms.worldToView[0][0]);
  e = glGetError();

  int textureIndex = 0;
  _bindTextureSamplerUniform(sprites.posX, GL_R32F, textureIndex++, data.mSceneShader.posX);
  e = glGetError();

  _bindTextureSamplerUniform(sprites.posY, GL_R32F, textureIndex++, data.mSceneShader.posY);
  e = glGetError();

  _bindTextureSamplerUniform(sprites.rotX, GL_R32F, textureIndex++, data.mSceneShader.rotX);
  e = glGetError();

  _bindTextureSamplerUniform(sprites.rotY, GL_R32F, textureIndex++, data.mSceneShader.rotY);
  e = glGetError();

  glDrawArraysInstanced(GL_TRIANGLES, 0, 6, GLsizei(sprites.count));
  e = glGetError();

  glUseProgram(0);
  e = glGetError();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  e = glGetError();

}

void ParticleRenderer::render(const ParticleData& data, const ParticleUniforms& uniforms, size_t frameIndex) {
  frameIndex;
  const TransformTargets target(0);
  e = glGetError();

  glUseProgram(data.mRenderShader.program);
  e = glGetError();

  //glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, data.mFeedbacks[target.dst]);
  //e = glGetError();
  //
  //glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, data.mFeedbacks[target.dst]);
  //e = glGetError();

  glBindBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[target.dst]);
  
  e = glGetError();
  //glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, data.mFeedbackBuffers[target.dst]);

  glEnableVertexAttribArray(0);
  e = glGetError();

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleData::Particle), 0);
  e = glGetError();

  uniforms;
  //glUniformMatrix4fv(data.mRenderShader.worldToView, 1, GL_FALSE, &uniforms.worldToView[0][0]);

  glDrawArrays(GL_POINTS, 0, 100);

  //glDrawTransformFeedback(GL_POINTS, data.mFeedbacks[target.dst]);
  e = glGetError();

  glDisableVertexAttribArray(0);
  glUseProgram(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  //glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
}
