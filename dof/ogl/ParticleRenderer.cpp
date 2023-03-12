#include "Precompile.h"
#include "ParticleRenderer.h"

#include "Shader.h"

const char* particleVS = R"(
  #version 330
  layout(location = 0) in vec2 mPosition;
  layout(location = 1) in vec2 mVelocity;
  layout(location = 2) in float mType;

  out vec2 mPosition0;
  out vec2 mVelocity0;
  out float mType0;

  void main() {
    mPosition0 = mPosition + mVelocity;
    mVelocity0 = mVelocity;
    mType0 = mType;
  }
)";

const char* particleGS = R"(
  #version 330
  layout(points) in;
  layout(points) out;
  layout(max_vertices = 100) out;

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
    //if(mType0[0] == 1.0) {
      EmitVertex();
      EndPrimitive();
    //}
  }
)";

const char* particleRenderVS = R"(
  #version 330
  layout(location = 0) in vec2 mPosition;
  void main() { gl_Position = vec4(mPosition, 0.0, 1.0); }
)";

const char* particleRenderPS = R"(
  #version 330 core
  out vec3 oColor;
  void main() { oColor = vec3(1, 0, 0); }
)";


namespace {
  struct TransformTargets {
    TransformTargets(size_t index)
      : src(index % 2)
      , dst((index + 1) % 2) {
    }
    size_t src = 0;
    size_t dst = 0;
  };
  void bindForUpdate(const ParticleData& data, const TransformTargets& targets) {
    glUseProgram(data.mFeedbackProgram);
    glEnable(GL_RASTERIZER_DISCARD);
    glBindBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[targets.src]);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, data.mFeedbacks[targets.dst]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleData::Particle), (const void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleData::Particle), (const void*)(sizeof(float)*2));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleData::Particle), (const void*)(sizeof(float)*4));

    glBeginTransformFeedback(GL_POINTS);
  }

  void unbindForUpdate() {
    glEndTransformFeedback();
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDisable(GL_RASTERIZER_DISCARD);
    glUseProgram(0);
  }
}


void ParticleRenderer::init(ParticleData& data) {
  auto e = glGetError();

  constexpr size_t particleCount = 10000;

  glGenTransformFeedbacks(data.mFeedbacks.size(), data.mFeedbacks.data());
  glGenBuffers(data.mFeedbackBuffers.size(), data.mFeedbackBuffers.data());
  std::vector<ParticleData::Particle> particles(particleCount);
  for(size_t i = 0; i < particles.size(); ++i) {
    particles[i].pos.x = float(i)*0.001f;
    particles[i].pos.y = float(i)*0.00001f;
    particles[i].vel.y = (i % 2) ? 0.001f : -0.001f;
  }
  for(size_t i = 0; i < data.mFeedbackBuffers.size(); ++i) {
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, data.mFeedbacks[i]);
    glBindBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[i]);
    particles[0].type = 1;
    glBufferData(GL_ARRAY_BUFFER, sizeof(ParticleData::Particle)*particles.size(), particles.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, data.mFeedbackBuffers[i]);
  }

  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  GLuint gs = glCreateShader(GL_GEOMETRY_SHADER);
  Shader::compileShader(vs, particleVS);
  Shader::compileShader(gs, particleGS);

  data.mFeedbackProgram = glCreateProgram();
  glAttachShader(data.mFeedbackProgram, vs);
  glAttachShader(data.mFeedbackProgram, gs);
  const GLchar* varyings[] = {
    "mPosition1",
    "mVelocity1",
    "mType1"
  };
  glTransformFeedbackVaryings(data.mFeedbackProgram, 3, varyings, GL_INTERLEAVED_ATTRIBS);
  e = glGetError();
  e = glGetError();

  if(!Shader::_linkAndValidate(data.mFeedbackProgram)) {
    printf("failed to created particle shader");
  }
  Shader::_detachAndDestroy(data.mFeedbackProgram, vs, gs);

  data.mRenderProgram = Shader::loadShader(particleRenderVS, particleRenderPS);

  //Populate initial feedback data
  bindForUpdate(data, TransformTargets(0));
  e = glGetError();

  glDrawArrays(GL_POINTS, 0, 1000);
  e = glGetError();

  unbindForUpdate();
  e = glGetError();
}

void ParticleRenderer::update(const ParticleData& data, size_t frameIndex) {
  auto e = glGetError();
  const TransformTargets target(frameIndex);

  bindForUpdate(data, target);
  e = glGetError();
  glDrawTransformFeedback(GL_POINTS, data.mFeedbacks[target.src]);
  e = glGetError();
  unbindForUpdate();
  e = glGetError();
}

void ParticleRenderer::render(const ParticleData& data, size_t frameIndex) {
  const TransformTargets target(frameIndex);
  glUseProgram(data.mRenderProgram);
  glBindBuffer(GL_ARRAY_BUFFER, data.mFeedbackBuffers[target.dst]);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleData::Particle), 0);

  glDrawTransformFeedback(GL_POINTS, data.mFeedbacks[target.dst]);

  glDisableVertexAttribArray(0);
  glUseProgram(0);
}
