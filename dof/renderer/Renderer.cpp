#include "Precompile.h"
#include "Renderer.h"

#include "CommonTasks.h"
#include "Queries.h"
#include "STBInterface.h"
#include "Table.h"

#include "glm/mat3x3.hpp"
#include "glm/ext/matrix_float3x3.hpp"

#include "glm/gtx/transform.hpp"

#include "sokol_gfx.h"

namespace DS {
  #include "shaders/DebugShader.h"
}
namespace TMS {
  #include "shaders/TexturedMeshShader.h"
}
#include "Debug.h"
#include "Quad.h"
#include "BlitPass.h"

namespace QuadPassTable {
  //TODO: from shader description
  using Transform = TMS::MW_t;
  using UVOffset = TMS::UV_t;

  struct TransformRow : Row<Transform>{};
  struct UVOffsetRow : Row<UVOffset>{};
  struct TintRow : Row<glm::vec4>{};
  struct IsImmobileRow : SharedRow<bool>{};
  struct PassRow : SharedRow<QuadPass>{};
  struct TextureIDRow : SharedRow<size_t>{};

  using Type = Table<
    TransformRow,
    UVOffsetRow,
    TintRow,
    IsImmobileRow,
    PassRow,
    TextureIDRow
  >;
};

namespace {
  bool isValid(const sg_image& image) {
    return sg_query_image_state(image) == SG_RESOURCESTATE_VALID;
  }

  struct RenderDebugDrawer {
    sg_pipeline pipeline{};
    sg_bindings bindings{};
  };

  struct OffscreenRender {
    sg_attachments attach{ 0 };
    sg_image target{ 0 };
  };

  struct RendererState {
    sg_pipeline texturedMeshPipeline;
    sg_buffer quadMesh;
    //Could be table but the amount of cameras isn't worth it
    std::vector<RendererCamera> mCameras;
    SceneState mSceneState;
    RenderDebugDrawer mDebug;
    sg_swapchain swapchain{};
    OffscreenRender offscreenRender;
    Blit::Pass blitTexturePass;
  };

  struct TextureRendererHandle {
    sg_image texture{};
  };

  glm::mat4 _getWorldToView(const RendererCamera& camera, float aspectRatio) {
    //TODO: not sure why this is inverse as glm says it wants x/y which is what the ratio is
    //Maybe I'm undoing it with a different transformation like part of glviewport or otherwise
    const float inverseAspect = 1.0f / aspectRatio;
    auto proj = camera.camera.orthographic ? glm::ortho(-inverseAspect, inverseAspect, -1.0f, 1.0f)
      : glm::perspective(glm::radians(camera.camera.fovDeg), inverseAspect, camera.camera.nearPlane, camera.camera.farPlane);
    //TODO: should be able to factor this into the view rather than needing a separate scale
    glm::vec3 scale = glm::vec3(camera.camera.zoom);
    return glm::inverse(
      glm::translate(glm::vec3(camera.pos.x, camera.pos.y, 0.0f)) *
      glm::rotate(camera.camera.angle, glm::vec3(0, 0, -1)) *
      glm::scale(scale) *
      proj
    );
  }

  sg_buffer _createQuadBuffers() {
    constexpr float s = 0.5f;
    constexpr float vertices[] = {
      -s,  s,    0.0f, 1.0f,
        s, s,   1.0f, 1.0f,
      s, -s,    1.0f, 0.0f,

      -s,  s,    0.0f, 1.0f,
      s, -s,      1.0f, 0.0f,
       -s, -s,   0.0f, 0.0f,
    };
    return sg_make_buffer(sg_buffer_desc{
      .data = SG_RANGE(vertices)
    });
  }

  RenderDebugDrawer _createDebugDrawer() {
    RenderDebugDrawer drawer;
    sg_pipeline_desc pipeline{
      .shader = sg_make_shader(DS::Debug_shader_desc(sg_query_backend())),
      .depth = sg_depth_state{
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .compare = SG_COMPAREFUNC_ALWAYS,
        .write_enabled = false
      },
      .color_count = 1,
    };
    pipeline.layout.attrs[ATTR_Debug_vertPos].format = SG_VERTEXFORMAT_FLOAT2;
    pipeline.layout.attrs[ATTR_Debug_vertColor].format = SG_VERTEXFORMAT_FLOAT3;
    pipeline.primitive_type = SG_PRIMITIVETYPE_LINES;
    sg_buffer_desc vb{
      .type = SG_BUFFERTYPE_VERTEXBUFFER,
      .usage = SG_USAGE_STREAM
    };
    vb.size = sizeof(float)*5*10000;
    drawer.bindings.vertex_buffers[0] = sg_make_buffer(vb);

    drawer.pipeline = sg_make_pipeline(pipeline);

    return drawer;
  }

  QuadUniforms _createQuadUniforms() {
    QuadUniforms result;
    constexpr size_t MAX_SIZE = 10000;
    sg_buffer_desc desc{
      .type = SG_BUFFERTYPE_STORAGEBUFFER,
      .usage = SG_USAGE_STREAM
    };
    desc.size = sizeof(TMS::MW_t) * MAX_SIZE;
    result.bindings.storage_buffers[SBUF_mw] = sg_make_buffer(desc);
    desc.size = sizeof(TMS::UV_t) * MAX_SIZE;
    result.bindings.storage_buffers[SBUF_uv] = sg_make_buffer(desc);
    desc.size = sizeof(TMS::TINT_t) * MAX_SIZE;
    result.bindings.storage_buffers[SBUF_tint] = sg_make_buffer(desc);
    result.bindings.samplers[SMP_sam] = sg_make_sampler(sg_sampler_desc{
      .min_filter = SG_FILTER_NEAREST,
      .mag_filter = SG_FILTER_NEAREST
    });
    return result;
  }

  sg_pipeline createTexturedMeshPipeline() {
    sg_pipeline_desc desc{ 0 };
    desc.shader = sg_make_shader(TMS::TexturedMesh_shader_desc(sg_query_backend()));
    desc.layout.attrs[ATTR_TexturedMesh_vertPos].format = SG_VERTEXFORMAT_FLOAT2;
    desc.layout.attrs[ATTR_TexturedMesh_vertUV].format = SG_VERTEXFORMAT_FLOAT2;
    desc.color_count = 1;
    desc.depth = sg_depth_state{
      .pixel_format = SG_PIXELFORMAT_DEPTH,
      .compare = SG_COMPAREFUNC_LESS,
      .write_enabled = true
    };
    return sg_make_pipeline(desc);
  }

  struct TexturesTuple {
    Row<TextureRendererHandle>* glHandle{};
    Row<TextureGameHandle>* gameHandle{};
  };

  void _loadTexture(TextureLoadRequest& request, TexturesTuple textures, ITableModifier& texturesModifier) {
    ImageData data = STBInterface::loadImageFromFile(request.mFileName.c_str(), 4);
    if(!data.mBytes) {
      request.mStatus = RequestStatus::Failed;
      return;
    }

    sg_image_desc desc{
      .width = (int)data.mWidth,
      .height = (int)data.mHeight,
      .pixel_format = SG_PIXELFORMAT_RGBA8
    };
    desc.data.subimage[0][0] = sg_range{ data.mBytes, data.mWidth*data.mHeight*4 };
    const sg_image image = sg_make_image(desc);

    size_t element = texturesModifier.addElements(1);
    textures.glHandle->at(element).texture = image;
    textures.gameHandle->at(element).mID = request.mImageID;

    request.mStatus = RequestStatus::Succeeded;

    STBInterface::deallocate(std::move(data));
  }

  void _processRequests(QueryResult<Row<TextureLoadRequest>>& requests,
    QueryResult<Row<TextureRendererHandle>, Row<TextureGameHandle>>& textures,
    ITableModifier& texturesModifier) {

    //Table should always exist and only one
    TexturesTuple tex{ std::get<0>(textures.rows).at(0), std::get<1>(textures.rows).at(0) };
    requests.forEachElement([&](TextureLoadRequest& request) {
      if(request.mStatus == RequestStatus::InProgress) {
        _loadTexture(request, tex, texturesModifier);
      }
    });
  }

  sg_image _getTextureByID(size_t id, QueryResult<const Row<TextureGameHandle>, const Row<TextureRendererHandle>>& textures) {
    for(size_t i = 0; i < textures.size(); ++i) {
      const auto& handles = textures.get<0>(i).mElements;
      for(size_t j = 0; j < handles.size(); ++j) {
        if(handles[j].mID == id) {
          return textures.get<1>(i).at(j).texture;
        }
      }
    }
    return {};
  }
}

struct RenderDB : IDatabase {
  using TexturesTable = Table<
    Row<TextureRendererHandle>,
    Row<TextureGameHandle>
  >;

  using GraphicsContext = Table<
    Row<RendererState>,
    Row<WindowData>
  >;

  using RendererDatabase = Database<
    GraphicsContext,
    TexturesTable,
    DebugLinePassTable::PointsTable,
    DebugLinePassTable::TextTable
  >;

  RenderDB(size_t quadPassCount, StableElementMappings& mappings)
    : runtime(getArgs(quadPassCount, mappings)) {
  }

  RuntimeDatabase& getRuntime() override {
    return runtime;
  }

  RuntimeDatabaseArgs getArgs(size_t quadPassCount, StableElementMappings& mappings) {
    RuntimeDatabaseArgs result;
    DBReflect::reflect(main, result, mappings);
    quadPasses.resize(quadPassCount);
    for(QuadPassTable::Type& pass : quadPasses) {
      DBReflect::addTable(pass, result, mappings);
    }
    return result;
  }

  RendererDatabase main;
  std::vector<QuadPassTable::Type> quadPasses;
  RuntimeDatabase runtime;
};

std::unique_ptr<IDatabase> Renderer::createDatabase(RuntimeDatabaseTaskBuilder&& builder, StableElementMappings& mappings) {
  auto sprites = builder.query<const Row<CubeSprite>>();
  const size_t quadPassCount = sprites.matchingTableIDs.size();
  //Create the database with the required number of quad pass tables
  auto result = std::make_unique<RenderDB>(quadPassCount, mappings);
  return result;
}

OffscreenRender createOffscreenRenderTarget(const sg_swapchain& swapchain) {
  sg_image_desc color{
    .render_target = true,
    .width = swapchain.width,
    .height = swapchain.height,
    .sample_count = swapchain.sample_count,
  };
  sg_image_desc depth{
    .render_target = true,
    .width = swapchain.width,
    .height = swapchain.height,
    .pixel_format = SG_PIXELFORMAT_DEPTH,
    .sample_count = swapchain.sample_count,
  };
  sg_attachments_desc att{ 0 };
  att.colors[0].image = sg_make_image(color);
  att.depth_stencil.image = sg_make_image(depth);

  return OffscreenRender{
    .attach = sg_make_attachments(att),
    .target = att.colors[0].image
  };
}

namespace Renderer {
  void initGame(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("renderer initGame").setPinning(AppTaskPinning::MainThread{});
    auto sprites = task.query<const Row<CubeSprite>>();
    auto resolver = task.getResolver<const IsImmobile>();
    auto quadPasses = task.query<QuadPassTable::PassRow, QuadPassTable::IsImmobileRow>();
    auto oglState = task.query<Row<RendererState>>();
    task.setCallback([sprites, resolver, quadPasses, oglState](AppTaskArgs&) mutable {
      RendererState& ogl = oglState.get<0>(0).at(0);
      //Should match based on createDatabase resizing to this
      assert(sprites.matchingTableIDs.size() == quadPasses.matchingTableIDs.size());

      ogl.texturedMeshPipeline = createTexturedMeshPipeline();
      ogl.quadMesh = _createQuadBuffers();
      ogl.mDebug = _createDebugDrawer();
      ogl.offscreenRender = createOffscreenRenderTarget(ogl.swapchain);
      ogl.blitTexturePass = Blit::createBlitTexturePass();

      //Fill in the quad pass tables
      for(size_t i = 0 ; i < sprites.matchingTableIDs.size(); ++i) {
        quadPasses.get<0>(i).at().mQuadUniforms = _createQuadUniforms();
        quadPasses.get<1>(i).at() = resolver->tryGetRow<const IsImmobile>(sprites.matchingTableIDs[i]) != nullptr;
      }
    });

    builder.submitTask(std::move(task));
  }
}

void Renderer::init(IAppBuilder& builder, const sg_swapchain& swapchain) {
  auto temp = builder.createTask();
  auto q = temp.query<Row<RendererState>>();
  assert(q.size());
  temp.getModifierForTable(q.matchingTableIDs[0])->resize(1);
  q.get<0>(0).at(0).swapchain = swapchain;
  temp.discard();

  initGame(builder);
}

void _renderDebug(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("renderDebug").setPinning(AppTaskPinning::MainThread{});
  auto globals = task.query<Row<RendererState>, const Row<WindowData>>();
  auto debugLines = task.query<const DebugLinePassTable::Points>();
  task.setCallback([globals, debugLines](AppTaskArgs&) mutable {
    PROFILE_SCOPE("renderer", "debug");
    RendererState* state = globals.tryGetSingletonElement<0>();
    const WindowData* window = globals.tryGetSingletonElement<1>();
    if(!state || !window) {
      return;
    }

    RenderDebugDrawer& debug = state->mDebug;
    for(size_t i = 0; i < debugLines.size(); ++i) {
      const auto& linesToDraw = debugLines.get<0>(i);
      if(linesToDraw.size()) {
        DS::DebugUniforms_t uniforms{};
        sg_apply_pipeline(debug.pipeline);
        sg_apply_bindings(debug.bindings);
        sg_update_buffer(debug.bindings.vertex_buffers[0], sg_range{ linesToDraw.data(), sizeof(DebugPoint)*linesToDraw.size() });

        for(const auto& renderCamera : state->mCameras) {
          glm::mat4 worldToView = _getWorldToView(renderCamera, window->aspectRatio);
          std::memcpy(uniforms.wvp, &worldToView, sizeof(worldToView));
          sg_apply_uniforms(UB_DebugUniforms, sg_range{ &uniforms, sizeof(uniforms) });

          sg_draw(0, static_cast<int>(linesToDraw.size()), 0);
        }
      }
    }
  });
  builder.submitTask(std::move(task));
}

void endPass(IAppBuilder& builder) {
  auto task = builder.createTask();
  auto globals = task.query<Row<RendererState>>();

  task.setCallback([globals](AppTaskArgs&) mutable {
    if(globals.tryGet<0>(0)) {
      sg_end_pass();
    }
  });

  builder.submitTask(std::move(task.setName("endPass").setPinning(AppTaskPinning::MainThread{})));
}

void extractTransform(IAppBuilder& builder, const TableID& src, const TableID& dst) {
  auto task = builder.createTask();

  QuadPassTable::TransformRow* transforms = task.query<QuadPassTable::TransformRow>(dst).tryGet<0>(0);
  auto srcQuery = task.query<
    const Tags::PosXRow, const Tags::PosYRow,
    const Tags::RotXRow, const Tags::RotYRow
  >(src);
  if(!transforms || !srcQuery.size()) {
    return task.discard();
  }
  const Tags::PosZRow* zRow = task.query<const Tags::PosZRow>(src).tryGet<0>(0);
  const Tags::ScaleXRow* xScaleRow = task.query<const Tags::ScaleXRow>(src).tryGet<0>(0);
  const Tags::ScaleYRow* yScaleRow = task.query<const Tags::ScaleYRow>(src).tryGet<0>(0);

  task.setCallback([=](AppTaskArgs&) mutable {
    auto [px, py, rx, ry] = srcQuery.get(0);
    //Previous task ensures that the sizes of src and dst match
    for(size_t i = 0; i < transforms->size(); ++i) {
      QuadPassTable::Transform& transform = transforms->at(i);
      transform.pos[0] = px->at(i);
      transform.pos[1] = py->at(i);
      if (zRow) {
        transform.pos[2] = zRow->at(i);
      }
      const float c = rx->at(i);
      const float s = ry->at(i);
      //2D rotation matrix in column major order
      float* m = transform.scaleRot;
      m[0] = c; m[2] = -s;
      m[1] = s; m[3] =  c;

      //Matrix multiply by scale one element at a time, skipping zeroes
      if(xScaleRow) {
        const float sx = xScaleRow->at(i);
        //[a b][x 0] = [ax b]
        //[c d][0 1]   [cx d]
        m[0] *= sx;
        m[1] *= sx;
      }

      if(yScaleRow) {
        const float sy = yScaleRow->at(i);
        //[a b][1 0] = [a by]
        //[c d][0 y]   [c dy]
        m[2] *= sy;
        m[3] *= sy;
      }
    }
  });

  builder.submitTask(std::move(task.setName("transform")));
}

//TODO: avoid when uvs haven't changed, which should be the common case
void extractUV(IAppBuilder& builder, const TableID& src, const TableID& dst) {
  auto task = builder.createTask();

  QuadPassTable::UVOffsetRow* dstRow = task.query<QuadPassTable::UVOffsetRow>(dst).tryGet<0>(0);
  const Row<CubeSprite>* srcRow = task.query<const Row<CubeSprite>>(src).tryGet<0>(0);
  if(!dstRow || !srcRow) {
    return task.discard();
  }

  task.setCallback([srcRow, dstRow](AppTaskArgs&) {
    for(size_t i = 0; i < dstRow->size(); ++i) {
      const CubeSprite& s = srcRow->at(i);
      QuadPassTable::UVOffset& d = dstRow->at(i);
      d.scale[0] = s.uMax - s.uMin;
      d.scale[1] = s.vMax - s.vMin;
      d.offset[0] = s.uMin;
      d.offset[1] = s.vMin;
    }
  });

  builder.submitTask(std::move(task.setName("uv")));
}

void Renderer::extractRenderables(IAppBuilder& builder) {
  auto temp = builder.createTask();
  temp.discard();
  auto sharedTextureSprites = temp.query<const Row<CubeSprite>, const SharedRow<TextureReference>>();
  auto globals = temp.query<Row<RendererState>>();
  auto passes = temp.query<QuadPassTable::PassRow>();
  assert(sharedTextureSprites.size() == passes.size());

  //Quads
  for(size_t pass = 0; pass < passes.size(); ++pass) {
    const TableID& passID = passes.matchingTableIDs[pass];
    const TableID& spriteID = sharedTextureSprites.matchingTableIDs[pass];

    //Resize the quad pass table to match the size of its paired sprite table
    {
      auto task = builder.createTask();
      task.setName("Resize Renderables");
      std::shared_ptr<ITableModifier> modifier = task.getModifierForTable(passes.matchingTableIDs[pass]);
      auto query = task.query<const Row<CubeSprite>>(spriteID);
      task.setCallback([modifier, query](AppTaskArgs&) mutable {
        modifier->resize(query.get<0>(0).size());
      });
      builder.submitTask(std::move(task));
    }
    //Copy each row of data to resized table
    extractTransform(builder, spriteID, passID);
    extractUV(builder, spriteID, passID);

    CommonTasks::moveOrCopyRowSameSize<SharedRow<TextureReference>, QuadPassTable::TextureIDRow>(builder, spriteID, passID);
    //Tint is optional
    if(temp.query<Tint>(spriteID).size()) {
      CommonTasks::moveOrCopyRowSameSize<Tint, QuadPassTable::TintRow>(builder, spriteID, passID);
    }
  }

  //Debug lines
  {
    auto task = builder.createTask();
    task.setName("extract debug");
    auto src = task.query<const Row<DebugPoint>>();
    auto dst = task.query<DebugLinePassTable::Points>();
    assert(src.size() == dst.size());
    auto modifiers = task.getModifiersForTables(dst.matchingTableIDs);
    task.setCallback([src, dst, modifiers](AppTaskArgs&) mutable {
      for(size_t i = 0; i < modifiers.size(); ++i) {
        auto& srcRow = src.get<0>(i);
        modifiers[i]->resize(srcRow.size());
        CommonTasks::Now::moveOrCopyRow(srcRow, dst.get<0>(i), 0);
      }
    });
    builder.submitTask(std::move(task));
  }
  //Debug text
  {
    auto task = builder.createTask();
    task.setName("extract text");
    auto src = task.query<const Row<DebugText>>();
    auto dst = task.query<DebugLinePassTable::Texts>();
    assert(src.size() == dst.size());
    auto modifiers = task.getModifiersForTables(dst.matchingTableIDs);
    task.setCallback([src, dst, modifiers](AppTaskArgs&) mutable {
      for(size_t i = 0; i < modifiers.size(); ++i) {
        auto& srcRow = src.get<0>(i);
        auto& dstRow = dst.get<0>(i);
        modifiers[i]->resize(srcRow.size());
        for(size_t e = 0; e < srcRow.size(); ++e) {
          const DebugText& srcE = srcRow.at(e);
          DebugLinePassTable::Text& dstE = dstRow.at(e);
          dstE.pos = srcE.pos;
          dstE.text = srcE.text;
        }
      }
    });
    builder.submitTask(std::move(task));
  }

  //Cameras
  {
    auto task = builder.createTask();
    auto src = task.query<const Row<Camera>,
      const FloatRow<Tags::Pos, Tags::X>,
      const FloatRow<Tags::Pos, Tags::Y>>();
    auto dst = task.query<Row<RendererState>>();
    assert(dst.size() == 1);
    task.setName("extract cameras");
    task.setCallback([src, dst](AppTaskArgs&) mutable {
      //Lazy since presumably there's just one
      RendererState* state = dst.tryGetSingletonElement();
      if(!state) {
        return;
      }
      state->mCameras.clear();
      src.forEachElement([state](const Camera& camera, const float& posX, const float& posY) {
        state->mCameras.push_back({ glm::vec2{ posX, posY }, camera });
      });
    });
    builder.submitTask(std::move(task));
  }

  //Globals
  {
    auto task = builder.createTask();
    auto src = task.query<const SharedRow<SceneState>>();
    auto dst = task.query<Row<RendererState>>();
    assert(dst.size() == 1);
    task.setName("extract globals");
    task.setCallback([src, dst](AppTaskArgs&) mutable {
      const SceneState* scene = src.tryGetSingletonElement();
      RendererState* state = dst.tryGetSingletonElement();
      if(scene && state) {
        state->mSceneState = *scene;
      }
    });
    builder.submitTask(std::move(task));
  }
}

void Renderer::clearRenderRequests(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("clear render requests");
  auto modifiers = task.getModifiersForTables(builder.queryTables<DebugClearPerFrame>().matchingTableIDs);
  task.setCallback([modifiers](AppTaskArgs&) {
    for(auto&& modifier : modifiers) {
      modifier->resize(0);
    }
  });

  builder.submitTask(std::move(task));
}

void Renderer::processRequests(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("Process Render Requests");
  auto requests = task.query<Row<TextureLoadRequest>>();
  auto textures = task.query<Row<TextureRendererHandle>, Row<TextureGameHandle>>();
  std::shared_ptr<ITableModifier> modifier{ task.getModifierForTable(textures.matchingTableIDs[0]) };
  task.setPinning(AppTaskPinning::MainThread{});
  task.setCallback([=](AppTaskArgs&) mutable {
    _processRequests(requests, textures, *modifier);
  });
  builder.submitTask(std::move(task).finalize());
}

void Renderer::render(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("Render");
  auto globals = task.query<Row<RendererState>, Row<WindowData>>();
  auto quads = task.query<const QuadPassTable::TransformRow,
    const QuadPassTable::UVOffsetRow,
    const QuadPassTable::TextureIDRow,
    const QuadPassTable::TintRow,
    QuadPassTable::PassRow>();
  auto textures = task.query<const Row<TextureGameHandle>, const Row<TextureRendererHandle>>();

  task.setPinning(AppTaskPinning::MainThread{});
  task.setCallback([globals, quads, textures](AppTaskArgs&) mutable {
    RendererState* state = globals.tryGetSingletonElement<0>();
    WindowData* window = globals.tryGetSingletonElement<1>();
    if(!state || !window) {
      return;
    }

    static bool first = true;
    static DebugRenderData debug = Debug::init();

    const float aspectRatio = window->mHeight ? float(window->mWidth)/float(window->mHeight) : 1.0f;
    window->aspectRatio = aspectRatio;

    auto& cameras = state->mCameras;
    for(auto& camera : cameras) {
      camera.worldToView = _getWorldToView(camera, aspectRatio);
    }
    sg_pass_action offscreenAction{
      .colors{
        sg_color_attachment_action{
          .load_action=SG_LOADACTION_CLEAR,
          .clear_value={0.0f, 0.0f, 1.0f, 1.0f }
        },
      }
    };
    sg_begin_pass(sg_pass{ .action = offscreenAction, .attachments = state->offscreenRender.attach });

    sg_apply_pipeline(state->texturedMeshPipeline);

    for(const auto& renderCamera : cameras) {
      PROFILE_SCOPE("renderer", "geometry");
      for(size_t i = 0; i < quads.size(); ++i) {
        auto [transforms, uvOffsets, textureIDs, tints, passes] = quads.get(i);
        QuadPass& pass = passes->at();

        size_t count = transforms->size();
        pass.mLastCount = count;
        if(!count) {
          continue;
        }

        const sg_image glTexture = _getTextureByID(textureIDs->at(), textures);
        if(!isValid(glTexture)) {
          continue;
        }
        pass.mQuadUniforms.bindings.images[IMG_tex] = glTexture;
        //TODO: use actual mesh instead of always quad
        pass.mQuadUniforms.bindings.vertex_buffers[0] = state->quadMesh;

        sg_update_buffer(pass.mQuadUniforms.bindings.storage_buffers[SBUF_mw], sg_range{ transforms->data(), sizeof(TMS::MW_t)*transforms->size() });
        //TODO: these rarely change, only upload when needed
        sg_update_buffer(pass.mQuadUniforms.bindings.storage_buffers[SBUF_tint], sg_range{ tints->data(), sizeof(TMS::TINT_t)*tints->size() });
        sg_update_buffer(pass.mQuadUniforms.bindings.storage_buffers[SBUF_uv], sg_range{ uvOffsets->data(), sizeof(TMS::UV_t)*uvOffsets->size() });

        const glm::mat4& worldToView = renderCamera.worldToView;

        TMS::uniforms_t uniforms{};
        std::memcpy(uniforms.worldToView, &worldToView, sizeof(glm::mat4));

        sg_apply_bindings(pass.mQuadUniforms.bindings);
        sg_apply_uniforms(UB_uniforms, sg_range{ &uniforms, sizeof(uniforms) });

        sg_draw(0, 6, transforms->size());
      }
    }

    /* TODO: move to simulation with imgui setting
    static bool renderBorders = true;
    if(renderBorders) {
      auto& debugTable = std::get<DebugLineTable>(db.mTables);
      const SceneState& scene = Simulation::_getSceneState(db);
      const glm::vec2 bl = scene.mBoundaryMin;
      const glm::vec2 ul = glm::vec2(scene.mBoundaryMin.x, scene.mBoundaryMax.y);
      const glm::vec2 ur = scene.mBoundaryMax;
      const glm::vec2 br = glm::vec2(scene.mBoundaryMax.x, scene.mBoundaryMin.y);

      std::array points{ bl, ul, ul, ur, ur, br, br, bl };
      for(const auto& p : points) {
        TableOperations::addToTable(debugTable).get<0>().mPos = p;
      }
    }
    */

    //Debug::pictureInPicture(debug, { 50, 50 }, { 350, 350 }, data.mSceneTexture);
  });
  builder.submitTask(std::move(task));

  _renderDebug(builder);

  endPass(builder);
}

void Renderer::commit(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("commit").setPinning(AppTaskPinning::MainThread{});
  auto query = task.query<Row<RendererState>>();
  task.setCallback([query](AppTaskArgs&) mutable {
    PROFILE_SCOPE("renderer", "commit");
    if(RendererState* state = query.tryGetSingletonElement()) {
      sg_pass_action action{
        .colors{
          sg_color_attachment_action{
            .load_action=SG_LOADACTION_DONTCARE,
          }
        }
      };

      //Copy the offscreen render to the screen
      sg_begin_pass(sg_pass{ .action = action, .swapchain = state->swapchain });
      Blit::blitTexture(Blit::Transform::fullScreen(), state->offscreenRender.target, state->blitTexturePass);
      sg_end_pass();

      sg_commit();
    }
  });
  builder.submitTask(std::move(task));
}