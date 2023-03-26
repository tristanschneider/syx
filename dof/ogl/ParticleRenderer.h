#pragma once

#include "GL/glew.h"
#include "glm/vec2.hpp"
#include "glm/mat4x4.hpp"
#include "Shader.h"
#include "VertexAttributes.h"

struct ParticleData {
  struct Particle {
    glm::vec2 pos{};
    glm::vec2 vel{};
    float type{};
  };

  struct ParticleAttributes : VertexAttributes<&Particle::pos, &Particle::vel, &Particle::type> {};

  //Two because feedback must read from and write to different locations, so they swap each frame
  //The transform feedback buffer
  std::array<GLuint, 2> mFeedbacks;
  //The raw particle data
  std::array<GLuint, 2> mFeedbackBuffers;
  struct UpdateShader {
    GLuint program{};
    GLint sceneTexture{};
  };
  struct RenderShader {
    GLuint program{};
    GLuint worldToView{};
  };
  UpdateShader mUpdateShader;
  RenderShader mRenderShader;

  struct SceneShader {
    GLuint mProgram{};
    GLuint posX, posY, rotX, rotY;
    GLuint worldToView;
  };
  struct ParticleSceneShader {
    GLuint mProgram{};
  };
  SceneShader mSceneShader;
  ParticleSceneShader mParticleSceneShader;
  GLuint mSceneFBO{};
  GLuint mSceneTexture{};

  static constexpr size_t SCENE_WIDTH = 1024;
  static constexpr size_t SCENE_HEIGHT = 1024;
};

struct CubeSpriteInfo {
  //Sampler buffers for quads used during normal rendering
  TextureSamplerUniform posX, posY, rotX, rotY;
  GLuint quadVertexBuffer;
  size_t count{};
};

struct ParticleUniforms {
  glm::mat4 worldToView{};
  //Transform world space to particle space, where particle space is centered around zero
  glm::mat4 worldToParticle;
  glm::mat4 particleToWorld;
};

struct ParticleRenderer {
  static void init(ParticleData& data);
  static void update(const ParticleData& data, const ParticleUniforms& uniforms, size_t frameIndex);
  static void renderNormalsBegin(const ParticleData& data);
  static void renderNormalsEnd();
  static void renderNormals(const ParticleData& data, const ParticleUniforms& uniforms, const CubeSpriteInfo& sprites);
  static void renderParticleNormals(const ParticleData& data, const ParticleUniforms& uniforms, size_t frameIndex);
  static void render(const ParticleData& data, const ParticleUniforms& uniforms, size_t frameIndex);
};