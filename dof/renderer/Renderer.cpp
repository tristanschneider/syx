#include "Precompile.h"
#include "Renderer.h"

#include "CommonTasks.h"
#include "Queries.h"
#include "STBInterface.h"
#include "Table.h"

#include "glm/mat3x3.hpp"
#include "glm/ext/matrix_float3x3.hpp"

#include "glm/gtx/transform.hpp"

#define SOKOL_GLCORE
#include "sokol_gfx.h"

namespace DS {
  #include "shaders/DebugShader.h"
}
namespace TMS {
  #include "shaders/TexturedMeshShader.h"
}
#include "Debug.h"
#include "Quad.h"
#include "QuadPassTable.h"

namespace {
  struct RenderDebugDrawer {
    sg_pipeline pipeline{};
    sg_bindings bindings;
  };

  struct RendererState {
    sg_pipeline texturedMeshPipeline;
    sg_buffer quadMesh;
    //Could be table but the amount of cameras isn't worth it
    std::vector<RendererCamera> mCameras;
    SceneState mSceneState;
    RenderDebugDrawer mDebug;
    sg_swapchain swapchain{};
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
    float vertices[] = {
        -0.5f,  0.5f,    0.0f, 0.0f,
         0.5f, -0.5f,    1.0f, 1.0f,
        -0.5f, -0.5f,    1.0f, 0.0f,

        -0.5f,  0.5f,    0.0f, 0.0f,
        0.5f, 0.5f,    0.0f, 1.0f,
         0.5f, -0.5f,    1.0f, 1.0f,
    };
    return sg_make_buffer(sg_buffer_desc{
      .data = SG_RANGE(vertices)
    });
  }

  RenderDebugDrawer _createDebugDrawer() {
    RenderDebugDrawer drawer;
    sg_pipeline_desc pipeline{
      .shader = sg_make_shader(DS::Debug_shader_desc(sg_query_backend()))
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
    return result;
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

namespace Renderer {
  sg_pipeline createTexturedMeshPipeline() {
    //TODO:
    return {};
  }

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

      //Fill in the quad pass tables
      for(size_t i = 0 ; i < sprites.matchingTableIDs.size(); ++i) {
        quadPasses.get<0>(i).at().mQuadUniforms = _createQuadUniforms();
        quadPasses.get<1>(i).at() = resolver->tryGetRow<const IsImmobile>(sprites.matchingTableIDs[i]) != nullptr;
      }
    });

    builder.submitTask(std::move(task));
  }
}

void Renderer::init(IAppBuilder& builder) {
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
    passID,spriteID;

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
    /*
    CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::Pos, Tags::X>, QuadPassTable::PosX>(builder, spriteID, passID);
    CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::Pos, Tags::Y>, QuadPassTable::PosY>(builder, spriteID, passID);
    CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::Rot, Tags::CosAngle>, QuadPassTable::RotX>(builder, spriteID, passID);
    CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::Rot, Tags::SinAngle>, QuadPassTable::RotY>(builder, spriteID, passID);
    CommonTasks::moveOrCopyRowSameSize<Row<CubeSprite>, QuadPassTable::UV>(builder, spriteID, passID);
    CommonTasks::moveOrCopyRowSameSize<SharedRow<TextureReference>, QuadPassTable::Texture>(builder, spriteID, passID);
    if(builder.queryTable<Tags::PosZRow>(spriteID)) {
      CommonTasks::moveOrCopyRowSameSize<Tags::PosZRow, QuadPassTable::PosZ>(builder, spriteID, passID);
    }
    if(builder.queryTable<Tags::ScaleXRow>(spriteID)) {
      CommonTasks::moveOrCopyRowSameSize<Tags::ScaleXRow, QuadPassTable::ScaleX>(builder, spriteID, passID);
    }
    if(builder.queryTable<Tags::ScaleYRow>(spriteID)) {
      CommonTasks::moveOrCopyRowSameSize<Tags::ScaleYRow, QuadPassTable::ScaleY>(builder, spriteID, passID);
    }
    //If this table has velocity, add those tasks as well
    if(temp.query<FloatRow<Tags::LinVel, Tags::X>, FloatRow<Tags::AngVel, Tags::Angle>>(spriteID).size()) {
      CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::LinVel, Tags::X>, QuadPassTable::LinVelX>(builder, spriteID, passID);
      CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::LinVel, Tags::Y>, QuadPassTable::LinVelY>(builder, spriteID, passID);
      CommonTasks::moveOrCopyRowSameSize<FloatRow<Tags::AngVel, Tags::Angle>, QuadPassTable::AngVel>(builder, spriteID, passID);
    }
    //Tint is also optional
    if(temp.query<Tint>(spriteID).size()) {
      CommonTasks::moveOrCopyRowSameSize<Tint, QuadPassTable::Tint>(builder, spriteID, passID);
    }
    */
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

    sg_pass_action action{
      .colors{
        sg_color_attachment_action{
          .load_action=SG_LOADACTION_CLEAR,
          .clear_value={0.0f, 0.0f, 1.0f, 1.0f }
        }
      }
    };
    sg_begin_pass(sg_pass{ .action = action, .swapchain = state->swapchain });

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

        //Will be zero and render as black if no valid texture is specified
        sg_image glTexture = _getTextureByID(textureIDs->at(), textures);

        //TODO: apply bindings of textured mesh pipeline

        const glm::mat4 worldToView = renderCamera.worldToView;

        // TODO: apply uniform

        // TODO: render 2 triangles, then add way to get size of mesh
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
}
