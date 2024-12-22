#include "Precompile.h"
#include "BlitPass.h"

#include "glm/glm.hpp"
#include "shaders/BlitTextureShader.h"

namespace Blit {
  Pass createBlitTexturePass() {
    sg_pipeline_desc pip{
      .shader = sg_make_shader(BlitTexture_shader_desc(sg_query_backend())),
    };
    pip.layout.attrs[ATTR_BlitTexture_vertPos].format = SG_VERTEXFORMAT_FLOAT2;
    pip.layout.attrs[ATTR_BlitTexture_vertUV].format = SG_VERTEXFORMAT_FLOAT2;

    sg_bindings bind{ 0 };
    constexpr float vertices[] = {
      -1.f,  1.f,    0.0f, 0.0f,
        1.f, 1.f,   1.0f, 0.0f,
      1.f, -1.f,    1.0f, 1.0f,

      -1.f,  1.f,    0.0f, 0.0f,
      1.f, -1.f,      1.0f, 1.0f,
       -1.f, -1.f,   0.0f, 1.0f,
    };

    bind.vertex_buffers[0] = sg_make_buffer(sg_buffer_desc{ .data = SG_RANGE(vertices) });
    bind.samplers[SMP_sam] = sg_make_sampler(sg_sampler_desc{
      .min_filter = SG_FILTER_LINEAR,
      .mag_filter = SG_FILTER_LINEAR
    });
    //Texture is assigned upon rendering

    return Pass{
      .pipeline = sg_make_pipeline(pip),
      .bindings = bind,
    };
  }

  void blitTexture(const Transform& transform, const sg_image& texture, Pass& pass) {
    pass.bindings.images[IMG_tex] = texture;
    sg_apply_pipeline(pass.pipeline);
    sg_apply_bindings(pass.bindings);

    uniforms_t uniforms{ 0 };
    glm::mat4 mat{ 0 };
    //Column major
    mat[0] = glm::vec4{ transform.size.x, 0, 0, 0 };
    mat[1] = glm::vec4{ 0, transform.size.y, 0, 0 };
    mat[2] = glm::vec4{ 0, 0, 1, 0 };
    mat[3] = glm::vec4{ transform.center.x, transform.center.y, 0, 1 };
    std::memcpy(&uniforms, &mat, sizeof(mat));
    sg_apply_uniforms(UB_uniforms, sg_range{ &uniforms, sizeof(uniforms) });

    sg_draw(0, 6, 1);
  }
}