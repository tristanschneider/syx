#include "Precompile.h"
#include "Renderer.h"

#include "CommonTasks.h"
#include "Table.h"

#include "glm/mat3x3.hpp"
#include "glm/ext/matrix_float3x3.hpp"

#include "glm/gtx/transform.hpp"

#include "sokol_gfx.h"
#include "Game.h"
#include "Events.h"

namespace DS {
  #include "shaders/DebugShader.h"
}
namespace TMS {
  #include "shaders/TexturedMeshShader.h"
}
#include "Quad.h"
#include "BlitPass.h"

#include "sokol_glue.h"
#include "FontPass.h"
#include "loader/SceneAsset.h"
#include "GraphicsTables.h"
#include "TLSTaskImpl.h"
#include <module/MassModule.h>

namespace QuadPassTable {
  using Transform = TMS::MW_t;
  using UVOffset = TMS::UV_t;

  struct TransformRow : Row<Transform>{};
  struct UVOffsetRow : Row<UVOffset>{};
  struct TintRow : Row<glm::vec4>{};
  struct IsImmobileRow : SharedRow<bool>{};
  struct PassRow : SharedRow<QuadPass>{};

  using Type = Table<
    TransformRow,
    UVOffsetRow,
    TintRow,
    IsImmobileRow,
    PassRow,
    SharedTextureRow,
    SharedMeshRow,
    MeshRow
  >;
};

namespace Renderer {
  //Creates the renderer database using information from the game database
  void createDatabase(RuntimeDatabaseArgs& args);
  //Called after creating the database and a window has been created
  void init(IAppBuilder& builder, const RendererContext& context);
  void extractRenderables(IAppBuilder& builder);
  void clearRenderRequests(IAppBuilder& builder);
  void render(IAppBuilder& builder);
  void endMainPass(IAppBuilder& builder);
  void commit(IAppBuilder& builder);
  void preProcessEvents(IAppBuilder& builder);
}

namespace {
  bool isValid(const sg_image& image) {
    return sg_query_image_state(image) == SG_RESOURCESTATE_VALID;
  }

  bool isValid(const sg_buffer& buff) {
    return sg_query_buffer_state(buff) == SG_RESOURCESTATE_VALID;
  }

  struct MeshGPUAsset {
    static bool isValid(const MeshGPUAsset* v) {
      return v && ::isValid(v->vertexBuffer);
    }

    sg_buffer vertexBuffer;
    size_t vertexCount{};
  };

  struct MaterialGPUAsset {
    static bool isValid(const MaterialGPUAsset* v) {
      return v && ::isValid(v->image);
    }

    sg_image image;
  };

  struct MeshGPUAssetRow : Row<MeshGPUAsset> {};
  struct MaterialGPUAssetRow : Row<MaterialGPUAsset> {};

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
    MeshGPUAsset quadMesh;
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

  MeshGPUAsset _createQuadBuffers() {
    constexpr float s = 0.5f;
    //Origin of texture is top left, might be OGL only
    constexpr float vertices[] = {
      -s,  s,    0.0f, 0.0f,
        s, s,   1.0f, 0.0f,
      s, -s,    1.0f, 1.0f,

      -s,  s,    0.0f, 0.0f,
      s, -s,      1.0f, 1.0f,
       -s, -s,   0.0f, 1.0f,
    };
    return MeshGPUAsset{
      .vertexBuffer = sg_make_buffer(sg_buffer_desc{
        .data = SG_RANGE(vertices)
      }),
      .vertexCount = 6
    };
  }

  constexpr size_t MAX_DEBUG_LINES = 10000;
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
    vb.size = sizeof(float)*5*MAX_DEBUG_LINES;
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
}

struct RenderDBStorage : ChainedRuntimeStorage {
  using ChainedRuntimeStorage::ChainedRuntimeStorage;

  using GraphicsContext = Table<
    Row<RendererState>,
    Row<WindowData>,
    FontPass::GlobalsRow
  >;

  using RendererDatabase = Database<
    GraphicsContext,
    DebugLinePassTable::PointsTable,
    DebugLinePassTable::TextTable
  >;

  RendererDatabase database;
  std::vector<QuadPassTable::Type> quadPasses;
  std::vector<MeshGPUAssetRow> meshGPU;
  std::vector<MaterialGPUAssetRow> materialGPU;
};

void Renderer::createDatabase(RuntimeDatabaseArgs& args) {
  RenderDBStorage* storage = RuntimeStorage::addToChain<RenderDBStorage>(args);
  //Add the base renderer database
  DBReflect::reflect(storage->database, args);

  //Inspect the existing tables for necessary modifications
  size_t quadPassCount{};
  std::vector<RuntimeTableRowBuilder*> materialTables, meshTables;
  for(RuntimeTableRowBuilder& table : args.tables) {
    if(table.contains<Row<CubeSprite>>()) {
      ++quadPassCount;
    }
    if(table.contains<Loader::MaterialAssetRow>()) {
      materialTables.push_back(&table);
    }
    if(table.contains<Loader::MeshAssetRow>()) {
      meshTables.push_back(&table);
    }
  }

  //Add a row to all tables with this asset for the GPU resources
  storage->meshGPU.resize(meshTables.size());
  for(size_t i = 0; i < meshTables.size(); ++i) {
    DBReflect::details::reflectRow(storage->meshGPU[i], *meshTables[i]);
  }

  storage->materialGPU.resize(materialTables.size());
  for(size_t i = 0; i < materialTables.size(); ++i) {
    DBReflect::details::reflectRow(storage->materialGPU[i], *materialTables[i]);
  }

  //Create our own tables that mirror the contents of quad passes
  storage->quadPasses.resize(quadPassCount);
  for(auto& pass : storage->quadPasses) {
    DBReflect::addTable(pass, args);
  }
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
    if(builder.getEnv().type == AppEnvType::InitThreadLocal) {
      return;
    }
    auto task = builder.createTask();
    task.setName("renderer initGame").setPinning(AppTaskPinning::MainThread{});
    auto sprites = task.query<const Row<CubeSprite>>();
    auto resolver = task.getResolver<const MassModule::IsImmobile>();
    auto quadPasses = task.query<QuadPassTable::PassRow, QuadPassTable::IsImmobileRow>();
    auto oglState = task.query<Row<RendererState>>();
    task.setCallback([sprites, resolver, quadPasses, oglState](AppTaskArgs&) mutable {
      RendererState& ogl = oglState.get<0>(0).at(0);
      //Should match based on createDatabase resizing to this
      assert(sprites.size() == quadPasses.size());

      ogl.texturedMeshPipeline = createTexturedMeshPipeline();
      ogl.quadMesh = _createQuadBuffers();
      ogl.mDebug = _createDebugDrawer();
      ogl.offscreenRender = createOffscreenRenderTarget(ogl.swapchain);
      ogl.blitTexturePass = Blit::createBlitTexturePass();

      //Fill in the quad pass tables
      for(size_t i = 0 ; i < sprites.size(); ++i) {
        quadPasses.get<0>(i).at().mQuadUniforms = _createQuadUniforms();
        quadPasses.get<1>(i).at() = resolver->tryGetRow<const MassModule::IsImmobile>(sprites[i]) != nullptr;
      }
    });

    builder.submitTask(std::move(task));
  }
}

void Renderer::init(IAppBuilder& builder, const RendererContext& context) {
  auto temp = builder.createTask();
  auto q = temp.query<Row<RendererState>>();
  assert(q.size());
  temp.getModifierForTable(q[0])->resize(1);
  RendererState& state = q.get<0>(0).at(0);
  FontPass::Globals* fontPass = temp.query<FontPass::GlobalsRow>().tryGetSingletonElement();
  assert(fontPass);
  state.swapchain = context.swapchain;
  fontPass->fontContext = context.fontContext;

  temp.discard();

  initGame(builder);

  FontPass::init(builder);
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
        const size_t elements = std::min(MAX_DEBUG_LINES, linesToDraw.size());
        sg_update_buffer(debug.bindings.vertex_buffers[0], sg_range{ linesToDraw.data(), sizeof(DebugPoint)*elements });

        for(const auto& renderCamera : state->mCameras) {
          const glm::mat4& worldToView = renderCamera.worldToView;
          std::memcpy(uniforms.wvp, &worldToView, sizeof(worldToView));
          sg_apply_uniforms(UB_DebugUniforms, sg_range{ &uniforms, sizeof(uniforms) });

          sg_draw(0, static_cast<int>(elements), 1);
        }
      }
    }
  });
  builder.submitTask(std::move(task));
}

void Renderer::endMainPass(IAppBuilder& builder) {
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
  auto sharedTextureSprites = temp.query<const Row<CubeSprite>, const SharedTextureRow>();
  auto globals = temp.query<Row<RendererState>>();
  auto passes = temp.query<QuadPassTable::PassRow>();
  assert(sharedTextureSprites.size() == passes.size());

  //Quads
  for(size_t pass = 0; pass < passes.size(); ++pass) {
    QuadPass& passConfig = passes.get<0>(pass).at();
    const TableID& passID = passes[pass];
    const TableID& spriteID = sharedTextureSprites[pass];

    //Resize the quad pass table to match the size of its paired sprite table
    {
      auto task = builder.createTask();
      task.setName("Resize Renderables");
      std::shared_ptr<ITableModifier> modifier = task.getModifierForTable(passes[pass]);
      auto query = task.query<const Row<CubeSprite>>(spriteID);
      task.setCallback([modifier, query](AppTaskArgs&) mutable {
        modifier->resize(query.get<0>(0).size());
      });
      builder.submitTask(std::move(task));
    }
    //Copy each row of data to resized table
    extractTransform(builder, spriteID, passID);
    extractUV(builder, spriteID, passID);

    CommonTasks::moveOrCopyRowSameSize<SharedTextureRow, SharedTextureRow>(builder, spriteID, passID);
    if(temp.query<SharedMeshRow>(spriteID).size()) {
      passConfig.sharedMesh = true;
      CommonTasks::moveOrCopyRowSameSize<SharedMeshRow, SharedMeshRow>(builder, spriteID, passID);
    }
    else if(temp.query<MeshRow>(spriteID).size()) {
      passConfig.sharedMesh = false;
      CommonTasks::moveOrCopyRowSameSize<MeshRow, MeshRow>(builder, spriteID, passID);
    }

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
    auto modifiers = task.getModifiersForTables(dst);
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
    auto modifiers = task.getModifiersForTables(dst);
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
  auto modifiers = task.getModifiersForTables(builder.queryTables<DebugClearPerFrame>());
  task.setCallback([modifiers](AppTaskArgs&) {
    for(auto&& modifier : modifiers) {
      modifier->resize(0);
    }
  });

  builder.submitTask(std::move(task));
}

struct RenderAssetReader {
  RenderAssetReader(RuntimeDatabaseTaskBuilder& task)
    : res{ task.getIDResolver()->getRefResolver() }
    , resolver{ task.getResolver(materials, meshes) }
  {
  }

  const MaterialGPUAsset* tryGetMaterial(const Loader::AssetHandle& handle) {
    return resolver->tryGetOrSwapRowElement(materials, res.tryUnpack(handle.asset));
  }

  const MeshGPUAsset* tryGetMesh(const Loader::AssetHandle& handle) {
    return resolver->tryGetOrSwapRowElement(meshes, res.tryUnpack(handle.asset));
  }

  ElementRefResolver res;
  CachedRow<const MaterialGPUAssetRow> materials;
  CachedRow<const MeshGPUAssetRow> meshes;
  std::shared_ptr<ITableResolver> resolver;
};

void onMaterialCreated(const Loader::MaterialAsset& cpu, MaterialGPUAsset& gpu) {
  const Loader::TextureAsset& tex = cpu.texture;
  sg_image_desc desc{
    .width = (int)tex.width,
    .height = (int)tex.height,
    .pixel_format = SG_PIXELFORMAT_RGBA8
  };
  desc.data.subimage[0][0] = sg_range{ tex.buffer.data(), tex.buffer.size() };
  gpu.image = sg_make_image(desc);
}

void onMaterialDestroyed(MaterialGPUAsset& gpu) {
  sg_destroy_image(gpu.image);
  gpu.image = {};
}

void onMeshCreated(const Loader::MeshAsset& cpu, MeshGPUAsset& gpu) {
  //These are already in the 2 pos 2 uv format that textured mesh pipeline expects
  gpu = MeshGPUAsset{
    .vertexBuffer = sg_make_buffer(sg_buffer_desc{
      .type = SG_BUFFERTYPE_VERTEXBUFFER,
      .usage = SG_USAGE_IMMUTABLE,
      .data = sg_range{ cpu.verts.data(), cpu.verts.size()*sizeof(Loader::MeshVertex) }
    }),
    .vertexCount = cpu.verts.size()
  };
}

void onMeshDestroyed(MeshGPUAsset& gpu) {
  sg_destroy_buffer(gpu.vertexBuffer);
  gpu.vertexBuffer = {};
}

struct AddRemoveAssets {
  void init(RuntimeDatabaseTaskBuilder& task) {
    materials = task;
    meshes = task;
    task.setPinning({ AppTaskPinning::MainThread{} });
  }

  void execute(AppTaskArgs&) {
    for(size_t t = 0; t < materials.size(); ++t) {
      auto [events, cpu, gpu] = materials.get(t);
      for(auto event : *events) {
        if(event.second.isDestroy()) {
          onMaterialDestroyed(gpu->at(event.first));
        }
        else if(event.second.isCreate()) {
          onMaterialCreated(cpu->at(event.first), gpu->at(event.first));
        }
        else {
          assert(false && "Unexpected for assets");
        }
      }
    }

    for(size_t t = 0; t < meshes.size(); ++t) {
      auto [events, cpu, gpu] = meshes.get(t);
      for(auto event : *events) {
        if(event.second.isDestroy()) {
          onMeshDestroyed(gpu->at(event.first));
        }
        else if(event.second.isCreate()) {
          onMeshCreated(cpu->at(event.first), gpu->at(event.first));
        }
        else {
          assert(false && "Unexpected for assets");
        }
      }
    }
  }

  QueryResult<const Events::EventsRow,
    const Loader::MaterialAssetRow,
    MaterialGPUAssetRow
  > materials;
  QueryResult<const Events::EventsRow,
    const Loader::MeshAssetRow,
    MeshGPUAssetRow
  > meshes;
};

//Monitor assets for GPU resources that need to be created or destroyed
void Renderer::preProcessEvents(IAppBuilder& builder) {
  builder.submitTask(TLSTask::create<AddRemoveAssets>("renderer assets"));
}

struct GetMeshOps {
  RenderAssetReader& assets;
  const SharedMeshRow& sharedMeshes;
  const MeshRow& individualMeshes;
  const MeshGPUAsset& fallback;
};

const MeshGPUAsset& getIndividualMesh(size_t e, const GetMeshOps& ops) {
  const MeshGPUAsset* meshAsset = ops.assets.tryGetMesh(ops.individualMeshes.at(e).asset);
  //Fall back to quad mesh if supplied mesh isn't valid
  return MeshGPUAsset::isValid(meshAsset) ? *meshAsset : ops.fallback;
}

const MeshGPUAsset& getSharedMesh(size_t, const GetMeshOps& ops) {
  const MeshGPUAsset* meshAsset = ops.assets.tryGetMesh(ops.sharedMeshes.at().asset);
  //Fall back to quad mesh if supplied mesh isn't valid
  return MeshGPUAsset::isValid(meshAsset) ? *meshAsset : ops.fallback;
}

void Renderer::render(IAppBuilder& builder) {
  auto task = builder.createTask();
  task.setName("Render");
  auto globals = task.query<Row<RendererState>, Row<WindowData>>();
  auto quads = task.query<const QuadPassTable::TransformRow,
    const QuadPassTable::UVOffsetRow,
    const SharedTextureRow,
    const SharedMeshRow,
    const MeshRow,
    const QuadPassTable::TintRow,
    QuadPassTable::PassRow>();
  RenderAssetReader assets{ task };

  task.setPinning(AppTaskPinning::MainThread{});
  task.setCallback([globals, quads, assets](AppTaskArgs&) mutable {
    RendererState* state = globals.tryGetSingletonElement<0>();
    WindowData* window = globals.tryGetSingletonElement<1>();
    if(!state || !window) {
      return;
    }

    const float aspectRatio = window->mHeight ? float(window->mWidth)/float(window->mHeight) : 1.0f;
    window->aspectRatio = aspectRatio;
    state->swapchain = sglue_swapchain();

    //Recreate render target if screen size changed so backing framebuffer always matches
    if(window->hasChanged) {
      sg_destroy_image(state->offscreenRender.target);
      sg_destroy_attachments(state->offscreenRender.attach);
      state->offscreenRender = createOffscreenRenderTarget(state->swapchain);
      window->hasChanged = false;
    }

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
        auto [transforms, uvOffsets, textureIDs, sharedMeshes, individualMeshes, tints, passes] = quads.get(i);
        QuadPass& pass = passes->at();

        size_t count = transforms->size();
        if(!count) {
          continue;
        }

        const MaterialGPUAsset* material = assets.tryGetMaterial(textureIDs->at().asset);
        //Don't render anything if material is missing
        if(!MaterialGPUAsset::isValid(material)) {
          continue;
        }

        const GetMeshOps meshOps{
          .assets = assets,
          .sharedMeshes = *sharedMeshes,
          .individualMeshes = *individualMeshes,
          .fallback = state->quadMesh
        };
        struct MeshInfo {
          decltype(&getSharedMesh) getMesh{};
          size_t stride{};
        };
        //Either draw the entire table at once as the same mesh, or draw each mesh one by one.
        //The is of course inefficient but good enough for now.
        const MeshInfo meshInfo = std::invoke([&]{
          return pass.sharedMesh ?
            MeshInfo{
              .getMesh = &getSharedMesh,
              .stride = transforms->size()
            }
            :
            MeshInfo{
              .getMesh = &getIndividualMesh,
              .stride = 1
            };
        });

        sg_update_buffer(pass.mQuadUniforms.bindings.storage_buffers[SBUF_mw], sg_range{ transforms->data(), sizeof(TMS::MW_t)*transforms->size() });
        //TODO: these rarely change, only upload when needed
        sg_update_buffer(pass.mQuadUniforms.bindings.storage_buffers[SBUF_tint], sg_range{ tints->data(), sizeof(TMS::TINT_t)*tints->size() });
        sg_update_buffer(pass.mQuadUniforms.bindings.storage_buffers[SBUF_uv], sg_range{ uvOffsets->data(), sizeof(TMS::UV_t)*uvOffsets->size() });

        for(size_t e = 0; e < transforms->size(); e += meshInfo.stride) {
          const MeshGPUAsset& mesh = meshInfo.getMesh(e, meshOps);

          pass.mQuadUniforms.bindings.images[IMG_tex] = material->image;
          pass.mQuadUniforms.bindings.vertex_buffers[0] = mesh.vertexBuffer;

          const glm::mat4& worldToView = renderCamera.worldToView;

          TMS::uniforms_t uniforms{};
          std::memcpy(uniforms.worldToView, &worldToView, sizeof(glm::mat4));
          uniforms.instanceOffset = static_cast<int>(e);

          sg_apply_bindings(pass.mQuadUniforms.bindings);
          sg_apply_uniforms(UB_uniforms, sg_range{ &uniforms, sizeof(uniforms) });

          sg_draw(0, static_cast<int>(mesh.vertexCount), meshInfo.stride);
        }
      }
    }
  });
  builder.submitTask(std::move(task));

  _renderDebug(builder);

  FontPass::render(builder);
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

void Renderer::injectRenderDependency(RuntimeDatabaseTaskBuilder& task) {
  task.query<Row<RendererState>>();
  task.setPinning(AppTaskPinning::MainThread{});
}

struct CameraReader : Renderer::ICameraReader {
  CameraReader(RuntimeDatabaseTaskBuilder& task)
    : query{ task.query<const Row<RendererState>>() }
    , window{ task.query<const Row<WindowData>>() }
  {
  }

  void getAll(std::vector<RendererCamera>& out) final {
    out.clear();
    for(size_t t = 0; t < query.size(); ++t) {
       for(const RendererState& s : query.get<0>(t)) {
         out.insert(out.end(), s.mCameras.begin(), s.mCameras.end());
       }
    }
  }

  WindowData getWindow() final {
    auto result = window.tryGetSingletonElement();
    return result ? *result : WindowData{};
  }

  QueryResult<const Row<RendererState>> query;
  QueryResult<const Row<WindowData>> window;
};

std::shared_ptr<Renderer::ICameraReader> Renderer::createCameraReader(RuntimeDatabaseTaskBuilder& task) {
  return std::make_unique<CameraReader>(task);
}

class RenderingModule : public IRenderingModule {
public:
  RenderingModule(const RendererContext& context)
    : ctx{ context }
  {
  }

  void createDependentDatabase(RuntimeDatabaseArgs& args) final {
    Renderer::createDatabase(args);
  }

  void renderOnlyUpdate(IAppBuilder& builder) final {
    Renderer::commit(builder);
  }

  void init(IAppBuilder& builder) final {
    Renderer::init(builder, ctx);
  }

  void preSimUpdate(IAppBuilder& builder) final {
    Renderer::extractRenderables(builder);
    Renderer::clearRenderRequests(builder);
    Renderer::render(builder);
  }

  void postSimUpdate(IAppBuilder& builder) final {
    Renderer::endMainPass(builder);
    Renderer::commit(builder);
  }

  void preProcessEvents(IAppBuilder& builder) final {
    Renderer::preProcessEvents(builder);
  }

  RendererContext ctx;
};

std::unique_ptr<IRenderingModule> Renderer::createModule(const RendererContext& context) {
  return std::make_unique<RenderingModule>(context);
}
