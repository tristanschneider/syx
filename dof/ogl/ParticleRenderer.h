#pragma once

#include "GL/glew.h"
#include "glm/vec2.hpp"

struct ParticleData {
  struct Particle {
    glm::vec2 pos{};
    glm::vec2 vel{};
    float type{};
  };

  //Two because feedback must read from and write to different locations, so they swap each frame
  //The transform feedback buffer
  std::array<GLuint, 2> mFeedbacks;
  //The raw particle data
  std::array<GLuint, 2> mFeedbackBuffers;
  GLuint mFeedbackProgram{};
  GLuint mRenderProgram{};
};

struct ParticleRenderer {
  static void init(ParticleData& data);
  static void update(const ParticleData& data, size_t frameIndex);
  static void render(const ParticleData& data, size_t frameIndex);
};