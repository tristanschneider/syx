#include "Precompile.h"
#include "FontPass.h"
#include "AppBuilder.h"
#include "Renderer.h"
#include "DebugLinePassTable.h"

#include "fontstash.h"
#include "sokol_gfx.h"
#include "util/sokol_fontstash.h"
#include "util/sokol_gl.h"
#include "DefaultFont.h"

#include "glm/gtx/transform.hpp"

namespace FontPass {
  Globals* getGlobals(RuntimeDatabaseTaskBuilder& task) {
    Globals* result = task.query<GlobalsRow>().tryGetSingletonElement();
    assert(result);
    return result;
  }

  void renderDebugText(IAppBuilder& builder) {
    auto task = builder.createTask();
    Renderer::injectRenderDependency(task);
    auto camera = Renderer::createCameraReader(task);
    auto query = task.query<const DebugLinePassTable::Texts>();
    Globals* globals = getGlobals(task);
    std::vector<RendererCamera> cameras;

    task.setCallback([globals, query, camera, cameras](AppTaskArgs&) mutable {
      sgl_defaults();
      fonsSetFont(globals->fontContext, globals->defaultFont);
      fonsSetSize(globals->fontContext, 100.f);
      fonsSetColor(globals->fontContext, sfons_rgba(255, 255, 255, 255));
      const WindowData window = camera->getWindow();
      sgl_matrix_mode_projection();

      const float inverseAspect = 1.0f / window.aspectRatio;
      //auto ortho = glm::ortho(0.0f, static_cast<float>(window.mWidth), 0.0f, static_cast<float>(window.mHeight), -10.0f, +10.0f);
      float m[16];
      //std::memcpy(&m, &ortho, sizeof(ortho));
      sgl_ortho(0.0f, static_cast<float>(window.mWidth), static_cast<float>(window.mHeight), 0.0f, -100.0f, +100.0f);
      //sgl_load_matrix(m);

      camera->getAll(cameras);
      for(const RendererCamera& cam : cameras) {
        std::memcpy(m, &cam.worldToView, sizeof(cam.worldToView));
        glm::mat4 test{
          1, 0, 0, 0,
          0, 1, 0, 0,
          0, 0, 1, 0,
          0, 0, 0, 1
        };

        //const float inverseAspect = 1.0f / window.aspectRatio;
        auto proj = cam.camera.orthographic ? glm::ortho(-inverseAspect, inverseAspect, -1.0f, 1.0f)
          : glm::perspective(glm::radians(cam.camera.fovDeg), inverseAspect, cam.camera.nearPlane, cam.camera.farPlane);
        //sgl_matrix_mode_projection();
        //std::memcpy(&m, &proj, sizeof(proj));
        //sgl_load_matrix(m);
        //proj = glm::inverse(proj);
        //sgl_matrix_mode_projection();
        //std::memcpy(m, &proj, sizeof(proj));
        //sgl_load_transpose_matrix(m);

        //test = cam.worldToView;
        sgl_matrix_mode_modelview();
        glm::vec3 scale = glm::vec3(cam.camera.zoom);
        static float z = 0;
        test = glm::inverse(
          glm::translate(glm::vec3(cam.pos.x, cam.pos.y, z)) *
          glm::rotate(cam.camera.angle, glm::vec3(0, 0, -1)) *
          glm::scale(scale)
          //proj
        );
        //test = cam.worldToView;
        std::memcpy(m, &test, sizeof(test));
        sgl_load_matrix(m);

        sgl_begin_lines();
        static float s = 1.f;
        static float ox = 0;
        static float oy = 0;
        sgl_v2f(0 + ox, 0 + oy);
        sgl_v2f(s + ox, 0 + oy);
        sgl_v2f(s + ox, s + oy);
        sgl_v2f(0 + ox, s + oy);
        sgl_end();

        for(size_t t = 0; t < query.size(); ++t) {
          for(const DebugLinePassTable::Text& text : query.get<0>(t).mElements) {
            glm::vec4 pos{ text.pos.x, text.pos.y, 0, 1 };
            //pos = test * pos;
            //TODO: actual position
            fonsDrawText(globals->fontContext, pos.x, pos.y, text.text.c_str(), nullptr);
          }
        }
        static glm::vec4 tp{ 0.5, 0.5, 0, 1 };
        //tp = test * tp;
        fonsDrawText(globals->fontContext, tp.x, tp.y, "Test Text", nullptr);
      }

      sfons_flush(globals->fontContext);
      sgl_draw();
    });

    builder.submitTask(std::move(task.setPinning(AppTaskPinning::MainThread{}).setName("font")));
  }

  void loadDefaultFont(IAppBuilder& builder) {
    auto task = builder.createTask();
    Globals* globals = getGlobals(task);

    task.setCallback([globals](AppTaskArgs&) {
      const std::string_view data = DefaultFont::getDefaultFont();
      globals->defaultFont = fonsAddFontMem(globals->fontContext, "default", reinterpret_cast<unsigned char*>(const_cast<char*>(data.data())), static_cast<int>(data.size()), 0);
    });

    //Font cache operations are allowed on any thread, only the final render needs to be main thread
    builder.submitTask(std::move(task.setName("font init")));
  }

  void init(IAppBuilder& builder) {
    loadDefaultFont(builder);
  }

  void render(IAppBuilder& builder) {
    renderDebugText(builder);
  }
}