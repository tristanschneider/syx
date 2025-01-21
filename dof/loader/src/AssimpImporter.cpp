#include "Precompile.h"
#include "AssimpImporter.h"

#include "IAssetImporter.h"
#include "AppBuilder.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "MeshRemapper.h"
#include "loader/SceneAsset.h"
#include "glm/glm.hpp"
#include "AssetLoadTask.h"
#include "AssetTables.h"
#include "MaterialImporter.h"

namespace Loader {
  struct NodeTraversal {
    const aiNode* node{};
    aiMatrix4x4 transform;
    size_t tableHash{};
  };

  struct SceneLoadContext {
    Assimp::Importer importer;
    AssetLoadTask& task;
    std::unique_ptr<AppTaskArgs> args;
    std::vector<NodeTraversal> nodesToTraverse;
    std::vector<const aiMetadata*> metaToTraverse;
    std::unique_ptr<MeshRemapper::IRemapping> meshMap;
    //Index from meshes stored temporarily here before meshMap is created
    std::vector<uint32_t> tempMeshMaterials;
  };

  std::string_view toView(const aiString& str) {
    return { str.data, str.length };
  }

  template<class T>
  concept MetadataReader = requires(T t, size_t hash, const aiMetadataEntry& data, SceneLoadContext& ctx) {
    t(hash, data, ctx);
  };

  template<MetadataReader Reader>
  void readMetadata(const aiNode& node, SceneLoadContext& context, const Reader& read) {
    context.metaToTraverse.push_back(node.mMetaData);
    while(!context.metaToTraverse.empty()) {
      const aiMetadata* meta = context.metaToTraverse.back();
      context.metaToTraverse.pop_back();
      if(!meta) {
        continue;
      }
      for(unsigned i = 0; i < meta->mNumProperties; ++i) {
        const size_t hash = gnx::Hash::constHash(std::string_view{ meta->mKeys[i].data, meta->mKeys[i].length });
        read(hash, meta->mValues[i], context);
      }
    }
  }

  //Custom properties don't exist on materials so they are hacked into the name with | delimiters
  template<class Reader>
  void readMaterialMetadata(const aiString& name, const Reader& read) {
    std::string_view view = toView(name);
    while(true) {
      if(size_t found = view.find('|'); found != view.npos) {
        std::string_view current = view.substr(0, found);
        view = view.substr(found + 1);

        read(gnx::Hash::constHash(current));
      }
      else {
        //Read the final section because a delimiter isn't required at the end: A|B should call for A and B
        if(view.size()) {
          read(gnx::Hash::constHash(view));
        }
        break;
      }
    }
  }

  glm::vec3 toVec3(const aiVector3D& v) {
    return { v.x, v.y, v.z };
  }

  glm::vec2 toVec2(const aiVector3D& v) {
    return { v.x, v.y };
  }

  //TODO: assuming axis/angle, is that right?
  float toRot(const aiVector3D& axis, ai_real angle) {
    return axis.z > 0 ? angle : -angle;
  }

  void load(const aiMatrix4x4& data, Transform3D& value) {
    aiVector3D scale, axis, translate;
    ai_real angle{};
    data.Decompose(scale, axis, angle, translate);
    value.pos = toVec3(translate);
    value.rot = toRot(axis, angle);
  }

  void load(const aiMatrix4x4& data, Transform3D& value, Scale2D& scaleValue) {
    aiVector3D scale, axis, translate;
    ai_real angle{};
    data.Decompose(scale, axis, angle, translate);
    value.pos = toVec3(translate);
    value.rot = toRot(axis, angle);
    scaleValue.scale = toVec2(scale);
  }

  //These are stored using a bool array property which parses as a metadata with an array of bool values
  void loadMask(const aiMetadataEntry& entry, uint8_t& mask) {
    if(entry.mType != AI_AIMETADATA) {
      return;
    }
    const aiMetadata* data = static_cast<aiMetadata*>(entry.mData);
    //Start at top bit going down so that the blender checkboxes go from most significant (top) to least (bottom)
    uint8_t currentMask = 1 << 7;
    for(unsigned i = 0; i < std::min((unsigned)8, data->mNumProperties); ++i) {
      const aiMetadataEntry& e = data->mValues[i];
      //Assume starting with 't' means this string is "true"
      if(const bool* b = static_cast<const bool*>(e.mData); e.mType == AI_BOOL && *b) {
        mask |= currentMask;
      }
      currentMask = currentMask >> 1;
    }
  }

  bool isNumberChar(char c) {
    return c == '-' || std::isdigit(static_cast<int>(c));
  }

  size_t loadSize(const aiMetadataEntry& e) {
    switch(e.mType) {
      case AI_INT32: return static_cast<size_t>(*static_cast<int32_t*>(e.mData));
      case AI_INT64: return static_cast<size_t>(*static_cast<int64_t*>(e.mData));
      case AI_UINT32: return static_cast<size_t>(*static_cast<uint32_t*>(e.mData));
      case AI_UINT64: return static_cast<size_t>(*static_cast<uint64_t*>(e.mData));
      //Float values don't make sense for size but probably more annoying to ignore than truncate
      case AI_FLOAT: return static_cast<size_t>(*static_cast<float*>(e.mData));
      case AI_DOUBLE: return static_cast<size_t>(*static_cast<double*>(e.mData));
      default: return 0;
    }
  }

  //These are stored as an array of  floats: [1.0, 1.0, 1.0, 1.0]
  glm::vec4 loadVec4(const aiMetadataEntry& e) {
    if(e.mType != AI_AIMETADATA) {
      return {};
    }
    const aiMetadata* data = static_cast<const aiMetadata*>(e.mData);
    glm::vec4 result{};
    for(unsigned i = 0; i < std::min(unsigned(4), data->mNumProperties); ++i) {
      double d{};
      data->Get(i, d);
      result[i] = static_cast<float>(d);
    }
    return result;
  }

  float loadFloat(const aiMetadataEntry& e) {
    switch(e.mType) {
      case AI_FLOAT: return *static_cast<const float*>(e.mData);
      case AI_DOUBLE: return static_cast<float>(*static_cast<const double*>(e.mData));
    }
    return {};
  }

  void load(const aiMetadataEntry& e, CollisionMask& mask) {
    loadMask(e, mask.mask);
  }

  void load(const aiMetadataEntry& e, ConstraintMask& mask) {
    loadMask(e, mask.mask);
  }

  void load(const aiMetadataEntry& e, Velocity3D& v) {
    const glm::vec4 raw = loadVec4(e);
    v.linear = { raw.x, raw.y, raw.z };
    v.angular = raw.w;
  }

  //Attempt to reference the mesh, assuming there is only one
  void load(const aiNode& e, MeshIndex& m, const SceneLoadContext& context) {
    if(!m.isSet() && e.mNumMeshes) {
      m = context.meshMap->remap(e.mMeshes[0]);
    }
  }

  void load(const aiMetadataEntry& e, Thickness& v) {
    v.thickness = loadFloat(e);
  }

  void loadPlayerElement(const NodeTraversal& node, SceneLoadContext& context, PlayerTable& player) {
    player.players.emplace_back();
    load(*node.node, player.meshIndex, context);
    Player& p = player.players.back();

    load(node.transform, p.transform);
    readMetadata(*node.node, context, [&p](size_t hash, const aiMetadataEntry& data, SceneLoadContext&) {
      switch(hash) {
        case(CollisionMask::KEY): return load(data, p.collisionMask);
        case(ConstraintMask::KEY): return load(data, p.constraintMask);
        case(Velocity3D::KEY): return load(data, p.velocity);
      }
    });
  }

  void loadPlayerTable(const NodeTraversal& node, SceneLoadContext& context, PlayerTable& player) {
    readMetadata(*node.node, context, [&player](size_t hash, const aiMetadataEntry& data, SceneLoadContext&) {
      switch(hash) {
        case(Thickness::KEY): return load(data, player.thickness);
      }
    });
  }

  void loadTerrainElement(const NodeTraversal& node, SceneLoadContext& context, TerrainTable& terrain) {
    terrain.terrains.emplace_back();
    load(*node.node, terrain.meshIndex, context);
    Terrain& t = terrain.terrains.back();

    load(node.transform, t.transform, t.scale);
    readMetadata(*node.node, context, [&t](size_t hash, const aiMetadataEntry& data, SceneLoadContext&) {
      switch(hash) {
        case(CollisionMask::KEY): return load(data, t.collisionMask);
        case(ConstraintMask::KEY): return load(data, t.constraintMask);
      }
    });
  }

  void loadTerrainTable(const NodeTraversal& node, SceneLoadContext& context, TerrainTable& terrain) {
    readMetadata(*node.node, context, [&terrain](size_t hash, const aiMetadataEntry& data, SceneLoadContext&) {
      switch(hash) {
        case(Thickness::KEY): return load(data, terrain.thickness);
      }
    });
  }

  void loadFragmentSpawnerElement(const NodeTraversal& node, SceneLoadContext& context, FragmentSpawnerTable& spawners) {
    spawners.spawners.emplace_back();
    FragmentSpawner& s = spawners.spawners.back();

    load(node.transform, s.transform, s.scale);
    load(*node.node, s.meshIndex, context);
    readMetadata(*node.node, context, [&](size_t hash, const aiMetadataEntry& data, SceneLoadContext&) {
      switch(hash) {
        case(FragmentCount::KEY): s.fragmentCount = { loadSize(data) }; break;
        case(CollisionMask::KEY): return load(data, s.collisionMask);
      }
    });
  }

  void loadFragmentSpawnerTable(const NodeTraversal& node, SceneLoadContext& context, FragmentSpawnerTable& spawner) {
    node;context;spawner;
  }

  void loadObject(size_t tableName, const NodeTraversal& node, SceneLoadContext& ctx, SceneAsset& scene) {
    switch(tableName) {
      case PlayerTable::KEY: return loadPlayerElement(node, ctx, scene.player);
      case TerrainTable::KEY: return loadTerrainElement(node, ctx, scene.terrain);
      case FragmentSpawnerTable::KEY: return loadFragmentSpawnerElement(node, ctx, scene.fragmentSpawners);
    }
  }

  void loadTable(size_t tableName, const NodeTraversal& node, SceneLoadContext& ctx, SceneAsset& scene) {
    switch(tableName) {
      case PlayerTable::KEY: return loadPlayerTable(node, ctx, scene.player);
      case TerrainTable::KEY: return loadTerrainTable(node, ctx, scene.terrain);
      case FragmentSpawnerTable::KEY: return loadFragmentSpawnerTable(node, ctx, scene.fragmentSpawners);
    }
  }

  void loadMaterialTask(const aiMaterial& mat, const aiScene& scene, AssetLoadTask& task) {
    aiString path;
    aiTextureMapping mapping{};
    unsigned uvIndex{};
    ai_real blend{};
    aiTextureOp op{};
    std::array<aiTextureMapMode, 3> mapMode{};
    path;mapping;uvIndex;blend;op;mapMode;

    MaterialImportSampleMode sampleMode = MaterialImportSampleMode::SnapToNearest;
    readMaterialMetadata(mat.GetName(), [&](size_t hash) {
      switch(hash) {
        case TEXTURE_SAMPLE_MODE_LINEAR_KEY:
          sampleMode = MaterialImportSampleMode::Linear;
          break;
        case TEXTURE_SAMPLE_MODE_SNAP_KEY:
          sampleMode = MaterialImportSampleMode::SnapToNearest;
          break;
      }
    });

    //Assume each material has a single texture if any, and that it's in the diffuse slot
    if(mat.GetTextureCount(aiTextureType_DIFFUSE) >= 1 && mat.GetTexture(aiTextureType_DIFFUSE, 0, &path, &mapping, &uvIndex, &blend, &op, mapMode.data()) == aiReturn_SUCCESS) {
      if(std::pair<const aiTexture*, int> tex = scene.GetEmbeddedTextureAndIndex(path.C_Str()); tex.first) {
        //If height is not provided it means the texture is in its raw format, use STB to parse that
        if(!tex.first->mHeight) {
          auto importer = Loader::createMaterialImporter(sampleMode);
          LoadRequest req;
          req.contents.resize(tex.first->mWidth);
          std::memcpy(req.contents.data(), tex.first->pcData, tex.first->mWidth);
          importer->loadAsset(req, task.asset);
        }
        //Otherwise the pixels exist as-is and can be copied over
        else {
          Loader::materialFromRaw(RawMaterial{
            //aiTexel always contains 4 components, target format is 4
            .bytes = reinterpret_cast<const uint8_t*>(tex.first->pcData),
            .width = tex.first->mWidth,
            .height = tex.first->mHeight,
            .sampleMode = sampleMode
          }, task.asset);
        }
      }
    }
    //Sometimes materials don't have textures, which is not a failure case. Skip this asset
    else {
      task.asset->emplace<EmptyAsset>();
    }
  }

  void loadMaterials(const aiScene& scene, SceneLoadContext& ctx, SceneAsset& result) {
    result.materials.resize(scene.mNumMaterials);
    for(unsigned i = 0; i < scene.mNumMaterials; ++i) {
      const aiMaterial* mat = scene.mMaterials[i];
      std::shared_ptr<AssetLoadTask> child = ctx.task.addTask(*ctx.args, [mat, &scene](AppTaskArgs&, AssetLoadTask& task) {
        loadMaterialTask(*mat, scene, task);
      });
      result.materials[i] = child->getAssetHandle();
      child->mDebug = "mat " + std::to_string(i);
    }
  }

  void loadMeshTask(const aiMesh& mesh, AssetLoadTask& task) {
    MeshAsset& resultMesh = task.asset->emplace<MeshAsset>();

    resultMesh.verts.resize(mesh.mNumFaces * 3);
    //Texture coordinates can be in any slot. Assume if they are provided they are in the first slot
    constexpr int EXPECTED_TEXTURE_CHANNEL = 0;
    const bool hasTexture = mesh.HasTextureCoords(EXPECTED_TEXTURE_CHANNEL);

    //Throw out the third dimension while copying over since assets are 2D, assume Z is unused, coordinates match the game where z is into the screen
    for(unsigned v = 0; v < mesh.mNumFaces; ++v) {
      const aiFace& face = mesh.mFaces[v];
      assert(face.mNumIndices == 3);
      const size_t base = v*3;
      for(unsigned fi = 0; fi < std::min(3u, face.mNumIndices); ++fi) {
        const unsigned vi = face.mIndices[fi];
        const aiVector3D& sourceVert = mesh.mVertices[vi];
        const aiVector3D& sourceUV = mesh.mTextureCoords[EXPECTED_TEXTURE_CHANNEL][vi];
        MeshVertex& mv = resultMesh.verts[base + fi];
        mv.pos = glm::vec2{ sourceVert.x, sourceVert.y };
        if(hasTexture) {
          mv.uv = glm::vec2{ sourceUV.x, sourceUV.y };
        }
      }
    }
  }

  void loadMeshes(const aiScene& scene, SceneLoadContext& ctx, SceneAsset& result) {
    result.meshes.resize(scene.mNumMeshes);
    ctx.tempMeshMaterials.resize(scene.mNumMeshes);

    for(unsigned i = 0; i < scene.mNumMeshes; ++i) {
      const aiMesh* mesh = scene.mMeshes[i];
      ctx.tempMeshMaterials[i] = mesh->mMaterialIndex;

      std::shared_ptr<AssetLoadTask> child = ctx.task.addTask(*ctx.args, [mesh](AppTaskArgs&, AssetLoadTask& task) {
        loadMeshTask(*mesh, task);
      });
      result.meshes[i] = child->getAssetHandle();
      child->mDebug = "mesh " + std::to_string(i);
    }
  }

  void awaitModelsAndMaterials(SceneLoadContext& ctx) {
    Loader::AssetLoadTask* toAwait = &ctx.task;
    while(toAwait) {
      //Await everything except this task itself
      if(&ctx.task != toAwait) {
        ctx.args->getScheduler()->awaitTasks(toAwait, 1, {});
      }
      toAwait = toAwait->next.get();
    }
  }

  struct ModelsAndMaterials {
    std::vector<MeshRemapper::RemapRef<MaterialAsset>> materials;
    std::vector<MeshRemapper::RemapRef<MeshAsset>> meshes;
  };

  AssetLoadTask* findTask(AssetLoadTask* head, const AssetHandle& toFind) {
    while(head) {
      if(head->getAssetHandle() == toFind) {
        return head;
      }
      head = head->next.get();
    }
    return nullptr;
  }

  void gatherModelsAndMaterials(SceneLoadContext& ctx, SceneAsset& scene, ModelsAndMaterials& result) {
    result.materials.resize(scene.materials.size());
    for(size_t i = 0; i < result.materials.size(); ++i) {
      AssetLoadTask* task = findTask(&ctx.task, scene.materials[i]);
      if(MaterialAsset* mat = task ? std::get_if<MaterialAsset>(&*task->asset) : nullptr) {
        result.materials[i] = MeshRemapper::RemapRef<MaterialAsset>{
          .handle = &task->getAssetHandle(),
          .value = mat
        };
      }
    }

    result.meshes.resize(scene.meshes.size());
    for(size_t i = 0; i < result.meshes.size(); ++i) {
      AssetLoadTask* task = findTask(&ctx.task, scene.meshes[i]);
      if(MeshAsset* mat = task ? std::get_if<MeshAsset>(&*task->asset) : nullptr) {
        result.meshes[i] = MeshRemapper::RemapRef<MeshAsset>{
          .handle = &task->getAssetHandle(),
          .value = mat
        };
      }
    }
  }

  void assignDeduplicatedModelsAndMaterials(SceneAsset& scene, ModelsAndMaterials& toAssign) {
    scene.materials.resize(toAssign.materials.size());
    std::transform(toAssign.materials.begin(), toAssign.materials.end(), scene.materials.begin(), MeshRemapper::RemapRefUnwrapper{});
  }

  void loadSceneAsset(const aiScene& scene, SceneLoadContext& ctx, SceneAsset& result) {
    ctx.task.mDebug = "main";
    //Enqueue all material/mesh loads
    loadMaterials(scene, ctx, result);
    loadMeshes(scene, ctx, result);

    //Await material/mesh to finish, then deduplicate the results
    awaitModelsAndMaterials(ctx);

    ModelsAndMaterials modelsAndMats;
    gatherModelsAndMaterials(ctx, result, modelsAndMats);

    //Compute the deduplicated results using the temporary ModelsAndMaterials container
    ctx.meshMap = MeshRemapper::createRemapping(modelsAndMats.meshes, ctx.tempMeshMaterials, modelsAndMats.materials);
    //Store the results of deduplication in the final result location from the temporary ModelsAndMaterials container
    assignDeduplicatedModelsAndMaterials(result, modelsAndMats);

    ctx.nodesToTraverse.push_back({ scene.mRootNode });
    while(ctx.nodesToTraverse.size()) {
      //Currently ignoring hierarchy, so depth or breadth first doesn't matter
      NodeTraversal node = ctx.nodesToTraverse.back();
      ctx.nodesToTraverse.pop_back();
      if(node.node) {
        //If this isn't a child of a table, try to find the table metadata and parse as table
        if(!node.tableHash) {
          readMetadata(*node.node, ctx, [&node](size_t hash, const aiMetadataEntry& data, SceneLoadContext&) {
           switch(hash) {
           case gnx::Hash::constHash("Table"):
             if(data.mType == AI_AISTRING) {
               node.tableHash = gnx::Hash::constHash(toView(*static_cast<const aiString*>(data.mData)));
             }
             break;
           }
          });

          if(node.tableHash) {
            loadTable(node.tableHash, node, ctx, result);
          }
        }
        else {
          loadObject(node.tableHash, node, ctx, result);
        }
        for(unsigned i = 0; i < node.node->mNumChildren; ++i) {
          if(const aiNode* child = node.node->mChildren[i]) {
            ctx.nodesToTraverse.push_back({ child, node.transform * child->mTransformation, node.tableHash });
          }
        }
      }
    }
  }

  class AssimpReaderImpl : public IAssetImporter {
  public:
    AssimpReaderImpl(AssetLoadTask& task, const AppTaskArgs& taskArgs)
      : ctx{ .task = task, .args = taskArgs.clone() }
    {
    }

    bool isSupportedExtension(std::string_view extension) final {
      return ctx.importer.IsExtensionSupported(std::string{ extension });
    }

    void loadAsset(const Loader::LoadRequest& request, AssetVariant&) final {
      const aiScene* scene = request.contents.size() ?
        ctx.importer.ReadFileFromMemory(request.contents.data(), request.contents.size(), 0) :
        ctx.importer.ReadFile(request.location.filename, 0);
      if(scene) {
        ctx.task.asset.v = SceneAsset{};
        loadSceneAsset(*scene, ctx, std::get<SceneAsset>(ctx.task.asset.v));
      }
    }

    SceneLoadContext ctx;
  };

  std::unique_ptr<IAssetImporter> createAssimpImporter(AssetLoadTask& task, const AppTaskArgs& taskArgs) {
    return std::make_unique<AssimpReaderImpl>(task, taskArgs);
  }
}