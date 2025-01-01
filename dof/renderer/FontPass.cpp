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

  void setMatrix(const glm::mat4& mat) {
    float m[16];
    std::memcpy(&m, &mat, sizeof(mat));
    sgl_load_matrix(m);
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
      const float fontSize = 100.f;
      const float fontScalar = 1/(fontSize*2.f);
      fonsSetSize(globals->fontContext, fontSize);
      fonsSetColor(globals->fontContext, sfons_rgba(255, 255, 255, 255));

      camera->getAll(cameras);
      for(const RendererCamera& cam : cameras) {
        sgl_matrix_mode_projection();
        setMatrix(cam.worldToView);

        sgl_matrix_mode_modelview();
        for(size_t t = 0; t < query.size(); ++t) {
          for(const DebugLinePassTable::Text& text : query.get<0>(t)) {
            // Font renders in pixels. Scale way down so it looks reasonable, then translate to world space
            setMatrix(glm::translate(glm::vec3{ text.pos.x, text.pos.y, 0 }) * glm::scale(glm::vec3(fontScalar, -fontScalar, fontScalar)));
            // Transform is in matrix stack meaning no position offset is needed here
            fonsDrawText(globals->fontContext, 0, 0, text.text.c_str(), nullptr);
          }
        }
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