#include "Precompile.h"

#include "AppBuilder.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "loader/AssetLoader.h"
#include "loader/SceneAsset.h"
#include "AssetTables.h"
#include "ILocalScheduler.h"
#include "glm/glm.hpp"
#include "generics/Hash.h"
#include "STBInterface.h"
#include "MeshRemapper.h"

#include "generics/RateLimiter.h"

namespace Loader {
  struct LoadFailure {};

  using AssetVariant = std::variant<
    std::monostate,
    LoadFailure,
    SceneAsset
  >;

  struct AssetOperations {
    using StoreFN = void(*)(RuntimeRow&, AssetVariant&&, size_t);
    QueryAliasBase destinationRow{};
    StoreFN writeToDestination{};
  };

  template<IsRow RowT, class AssetT>
  AssetOperations createAssetOperations() {
    struct Write {
      static void write(RuntimeRow& dst, AssetVariant&& toMove, size_t i) {
        static_assert(std::is_same_v<AssetT, typename RowT::ElementT>);
        static_cast<RowT*>(dst.row)->at(i) = std::move(std::get<AssetT>(toMove));
      }
    };

    return AssetOperations {
      .destinationRow{ QueryAlias<RowT>::create() },
      .writeToDestination{ &Write::write }
    };
  }

  struct GetAssetOperations {
    AssetOperations operator()(std::monostate) const { return {}; }
    AssetOperations operator()(const LoadFailure&) const { return {}; }
    AssetOperations operator()(const SceneAsset&) const {
      return createAssetOperations<SceneAssetRow, SceneAsset>();
    };
  };

  struct LoadingAsset {
    std::shared_ptr<AssetVariant> asset;
    std::shared_ptr<Tasks::ILongTask> task;
    LoadState state;
  };
  struct LoadingAssetRow : Row<LoadingAsset> {};

  class AssetIndex {
  public:
    ElementRef find(const AssetLocation& key) const {
      std::shared_lock lock{ mutex };
      auto it = index.find(key);
      return it != index.end() ? it->second : ElementRef{};
    }

    void insert(AssetLocation&& key, const ElementRef& value) {
      std::unique_lock lock{ mutex };
      index.emplace(std::make_pair(std::move(key), value));
    }

    void erase(const AssetLocation& key) {
      std::unique_lock lock{ mutex };
      index.erase(key);
    }

  private:
    mutable std::shared_mutex mutex;
    std::unordered_map<AssetLocation, ElementRef> index;
  };
  struct AssetIndexRow : SharedRow<AssetIndex> {};
  namespace db {
    using LoadingAssetTable = Table<
      StableIDRow,
      LoadingTagRow,
      UsageTrackerBlockRow,
      LoadingAssetRow
    >;
    template<class T>
    using SucceededAssetTable = Table<
      StableIDRow,
      SucceededTagRow,
      UsageTrackerBlockRow,
      T
    >;
    using LoaderDB = Database<
      Table<GlobalsRow>,
      Table<
        StableIDRow,
        RequestedTagRow,
        LoadRequestRow,
        UsageTrackerBlockRow
      >,
      Table<
        StableIDRow,
        AssetIndexRow,
        FailedTagRow,
        UsageTrackerBlockRow
      >,
      LoadingAssetTable,
      SucceededAssetTable<SceneAssetRow>
    >;
  }

  std::string toString(const aiMetadataEntry& entry) {
    void* data = entry.mData;
    switch(entry.mType) {
      case aiMetadataType::AI_BOOL:
        return *static_cast<bool*>(data) ? "true" : "false";
      case aiMetadataType::AI_INT32:
        return std::to_string(*static_cast<int32_t*>(data));
      case aiMetadataType::AI_UINT64:
        return std::to_string(*static_cast<uint64_t*>(data));
      case aiMetadataType::AI_FLOAT:
        return std::to_string(*static_cast<float*>(data));
      case aiMetadataType::AI_DOUBLE:
        return std::to_string(*static_cast<double*>(data));
      case aiMetadataType::AI_AISTRING:
        return static_cast<aiString*>(data)->C_Str();
      //In blender this is a float array of size 3
      case aiMetadataType::AI_AIVECTOR3D: {
        const auto v = static_cast<aiVector3D*>(data);
        return std::to_string(v->x) + ", " + std::to_string(v->y) + ", " + std::to_string(v->z);
      }
      case aiMetadataType::AI_AIMETADATA:
        return "meta";
      case aiMetadataType::AI_INT64:
        return std::to_string(*static_cast<int64_t*>(data));
      case aiMetadataType::AI_UINT32:
        return std::to_string(*static_cast<uint32_t*>(data));
      default:
        return "unknown";
    }
  }

  void printMetadata(std::vector<aiMetadata*>& meta) {
    while(meta.size()) {
      const aiMetadata* m = meta.back();
      meta.pop_back();
      if(!m) {
        continue;
      }
      for(unsigned i = 0; i < m->mNumProperties; ++i) {
        const aiString& key = m->mKeys[i];
        const aiMetadataEntry& value = m->mValues[i];
        std::string debug = toString(value);
        if(value.mType == aiMetadataType::AI_AIMETADATA) {
          meta.push_back(static_cast<aiMetadata*>(value.mData));
        }
        printf("%s -> %s\n", key.C_Str(), debug.c_str());
      }
    }
  }

  void printMetadata(const aiMetadata* meta) {
    if(meta) {
      std::vector<aiMetadata*> m;
      m.push_back((aiMetadata*)meta);
      printMetadata(m);
    }
  }

  //Assets exported in fbx format from blender with the "Custom Properties" value checked
  //exports with metadata that can specify game-related data
  void testLoadAsset() {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile("C:/syx/dof/build_output/Debug/test.fbx", 0);
    std::vector<aiNode*> traverse;
    std::vector<aiMetadata*> meta;
    traverse.push_back(scene->mRootNode);

    meta.push_back(scene->mMetaData);
    printMetadata(meta);

    while(traverse.size()) {
      const aiNode* node = traverse.back();
      traverse.pop_back();
      if(!node) {
        continue;
      }

      printf("%s\n", node->mName.C_Str());
      meta.push_back(node->mMetaData);
      printMetadata(meta);

      for(unsigned i = 0; i < node->mNumChildren; ++i) {
        traverse.push_back(node->mChildren[i]);
      }
      printf("\n");
    }

    __debugbreak();
    scene;
  }

  std::unique_ptr<IDatabase> createDB(StableElementMappings& mappings) {
    return DBReflect::createDatabase<db::LoaderDB>(mappings);
  }

  struct KnownTables {
    KnownTables(RuntimeDatabaseTaskBuilder& task)
      : requests{ task.queryTables<RequestedTagRow>()[0] }
      , loading{ task.queryTables<LoadingTagRow>()[0] }
      , failed{ task.queryTables<FailedTagRow>()[0] }
      , succeeded(std::move(task.queryTables<SucceededTagRow>().matchingTableIDs))
    {
    }

    TableID requests;
    TableID loading;
    TableID failed;
    std::vector<TableID> succeeded;
  };

  std::string_view getExtension(const std::string& str) {
    const size_t last = str.find_last_of('.');
    return last == str.npos ? std::string_view{} : std::string_view{ str.begin() + last + 1, str.end() };
  }

  struct NodeTraversal {
    const aiNode* node{};
    aiMatrix4x4 transform;
    size_t tableHash{};
  };

  struct SceneLoadContext {
    const AssetIndex& index;
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
        //Read the final section because a delimiter isn't rquired at the end: A|B should call for A and B
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

  void loadObject(size_t tableName, const NodeTraversal& node, SceneLoadContext& ctx, SceneAsset& scene) {
    switch(tableName) {
      case PlayerTable::KEY: return loadPlayerElement(node, ctx, scene.player);
      case TerrainTable::KEY: return loadTerrainElement(node, ctx, scene.terrain);
    }
  }

  void loadTable(size_t tableName, const NodeTraversal& node, SceneLoadContext& ctx, SceneAsset& scene) {
    switch(tableName) {
      case PlayerTable::KEY: return loadPlayerTable(node, ctx, scene.player);
      case TerrainTable::KEY: return loadTerrainTable(node, ctx, scene.terrain);
    }
  }

  void loadMaterials(const aiScene& scene, SceneLoadContext&, SceneAsset& result) {
    result.materials.resize(scene.mNumMaterials);
    for(unsigned i = 0; i < scene.mNumMaterials; ++i) {
      const aiMaterial* mat = scene.mMaterials[i];
      aiString path;
      aiTextureMapping mapping{};
      unsigned uvIndex{};
      ai_real blend{};
      aiTextureOp op{};
      std::array<aiTextureMapMode, 3> mapMode{};
      path;mapping;uvIndex;blend;op;mapMode;

      std::string temp;
      for(unsigned z = 0; z < mat->mNumProperties; ++z) {
        switch(mat->mProperties[z]->mType) {
          case aiPropertyTypeInfo::aiPTI_Buffer:
             temp += mat->mProperties[z]->mKey.C_Str() + std::string("|") + std::string("[b]\n");
             break;
          case aiPropertyTypeInfo::aiPTI_Double:
             temp += mat->mProperties[z]->mKey.C_Str() + std::string("|") + std::to_string(*(double*)mat->mProperties[z]->mData) + std::string("[d]\n");
             break;
          case aiPropertyTypeInfo::aiPTI_Float:
            temp += mat->mProperties[z]->mKey.C_Str() + std::string("|") + std::to_string(*(float*)mat->mProperties[z]->mData) + std::string("[f]\n");
             break;
          case aiPropertyTypeInfo::aiPTI_Integer:
            temp += mat->mProperties[z]->mKey.C_Str() + std::string("|") + std::to_string(*(int*)mat->mProperties[z]->mData) + std::string("[i]\n");
             break;
          case aiPropertyTypeInfo::aiPTI_String:
             temp += mat->mProperties[z]->mKey.C_Str() + std::string("|") + std::string(((aiString*)mat->mProperties[z]->mData)->C_Str()) + std::string("[s]\n");
             break;
        }
      }

      TextureAsset& resultTexture = result.materials[i].texture;
      readMaterialMetadata(mat->GetName(), [&resultTexture](size_t hash) {
        switch(hash) {
          case TEXTURE_SAMPLE_MODE_LINEAR_KEY:
            resultTexture.sampleMode = TextureSampleMode::LinearInterpolation;
            break;
          case TEXTURE_SAMPLE_MODE_SNAP_KEY:
            resultTexture.sampleMode = TextureSampleMode::SnapToNearest;
            break;
        }
      });

      //Assume each material has a single texture if any, and that it's in the diffuse slot
      if(mat->GetTextureCount(aiTextureType_DIFFUSE) >= 1 && mat->GetTexture(aiTextureType_DIFFUSE, 0, &path, &mapping, &uvIndex, &blend, &op, mapMode.data()) == aiReturn_SUCCESS) {
        if(std::pair<const aiTexture*, int> tex = scene.GetEmbeddedTextureAndIndex(path.C_Str()); tex.first) {
          constexpr int CHANNELS = 3;
          //If height is not provided it means the texture is in its raw format, use STB to parse that
          if(!tex.first->mHeight) {
            if(ImageData data = STBInterface::loadImageFromBuffer((const unsigned char*)tex.first->pcData, tex.first->mWidth, CHANNELS); data.mBytes) {
              TextureAsset& t = result.materials[i].texture;
              t.format = TextureFormat::RGB;
              t.width = data.mWidth;
              t.height = data.mHeight;
              t.buffer.resize(t.width*t.height*CHANNELS);
              std::memcpy(t.buffer.data(), data.mBytes, t.buffer.size());
              STBInterface::deallocate(std::move(data));
            }
          }
          //Otherwise the pixels exist as-is and can be copied over
          else {
            TextureAsset& t = result.materials[i].texture;
            t.format = TextureFormat::RGB;
            t.width = tex.first->mWidth;
            t.height = tex.first->mHeight;
            t.buffer.reserve(t.width*t.height*CHANNELS);
            t.buffer.resize(t.width*t.height*CHANNELS);
            const size_t pixels = t.width*t.height;
            for(size_t p = 0; p < pixels; ++p) {
              //aiTexel always contains 4 components, target format is 3
              const size_t dst = p * 3;
              const aiTexel& texel = tex.first->pcData[p];
              t.buffer[dst + 0] = texel.r;
              t.buffer[dst + 1] = texel.g;
              t.buffer[dst + 2] = texel.b;
            }
          }
        }
      }
    }
  }

  void loadMeshes(const aiScene& scene, SceneLoadContext& ctx, SceneAsset& result) {
    result.meshVertices.resize(scene.mNumMeshes);
    result.meshUVs.resize(scene.mNumMeshes);
    ctx.tempMeshMaterials.resize(scene.mNumMeshes);

    for(unsigned i = 0; i < scene.mNumMeshes; ++i) {
      const aiMesh* mesh = scene.mMeshes[i];

      MeshVerticesAsset& verts = result.meshVertices[i];
      MeshUVsAsset& uvs = result.meshUVs[i];
      ctx.tempMeshMaterials[i] = mesh->mMaterialIndex;

      verts.vertices.resize(mesh->mNumVertices);
      uvs.textureCoordinates.resize(mesh->mNumVertices);
      //Throw out the third dimension while copying over since assets are 2D, assume Z is unused, coordinates match the game where z is into the screen
      for(unsigned v = 0; v < mesh->mNumVertices; ++v) {
        const aiVector3D& sv = mesh->mVertices[v];
        verts.vertices[v] = glm::vec2{ sv.x, sv.y };
      }
      //Texture coordinates can be in any slot. Assume if they are provided they are in the first slot
      constexpr int EXPECTED_TEXTURE_CHANNEL = 0;
      if(mesh->HasTextureCoords(EXPECTED_TEXTURE_CHANNEL)) {
        for(unsigned v = 0; v < mesh->mNumVertices; ++v) {
          const aiVector3D& su = mesh->mTextureCoords[EXPECTED_TEXTURE_CHANNEL][v];
          uvs.textureCoordinates[v] = glm::vec2{ su.x, su.y };
        }
      }
    }
  }

  void loadScene(const aiScene& scene, SceneLoadContext& ctx, SceneAsset& result) {
    loadMaterials(scene, ctx, result);
    loadMeshes(scene, ctx, result);
    ctx.meshMap = MeshRemapper::createRemapping(result.meshVertices, result.meshUVs, ctx.tempMeshMaterials, result.materials);

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

  void loadAsset(const LoadRequest& request, AssetVariant& result, const AssetIndex& index) {
    result = LoadFailure{};

    Assimp::Importer importer;
    const std::string_view ext{ getExtension(request.location.filename) };
    if(importer.IsExtensionSupported(std::string{ ext })) {
      const aiScene* scene = request.contents.size() ?
        importer.ReadFileFromMemory(request.contents.data(), request.contents.size(), 0) :
        importer.ReadFile(request.location.filename, 0);
      if(scene) {
        result = SceneAsset{};
        SceneLoadContext context{ index };
        loadScene(*scene, context, std::get<SceneAsset>(result));
      }
      else {
        printf("No loader implemented for request [%s]\n", request.location.filename.c_str());
      }
    }
    else {
      printf("No loader implemented for request [%s]\n", request.location.filename.c_str());
    }
  }

  //Queue tasks for all requests in the requests table
  void startRequests(IAppBuilder& builder) {
    auto task = builder.createTask();
    KnownTables tables{ task };
    auto sourceQuery = task.query<const LoadRequestRow>(tables.requests);
    auto destinationQuery = task.query<LoadingAssetRow>(tables.loading);
    const AssetIndex* index = task.query<const AssetIndexRow>().tryGetSingletonElement();
    assert(index);
    RuntimeDatabase& db = task.getDatabase();
    RuntimeTable* loadingTable = db.tryGet(tables.loading);

    task.setCallback([=, &db](AppTaskArgs& args) mutable {
      auto& dq = destinationQuery.get<0>(0);
      for(size_t t = 0; t < sourceQuery.size(); ++t) {
        auto [source] = sourceQuery.get(t);
        RuntimeTable* sourceTable = db.tryGet(sourceQuery.matchingTableIDs[t]);
        if(!sourceTable) {
          continue;
        }
        while(source->size()) {
          const LoadRequest request = source->at(0);
          const size_t newIndex = dq.size();

          //Create the entry in the destination and remove the current
          RuntimeTable::migrateOne(0, *sourceTable, *loadingTable);
          LoadingAsset& newAsset = dq.at(newIndex);
          //Initialize task metadata and start the task
          newAsset.state.step = Loader::LoadStep::Loading;
          newAsset.asset = std::make_shared<AssetVariant>();
          newAsset.task = args.scheduler->queueLongTask([request, dst{newAsset.asset}, index](AppTaskArgs&) {
            loadAsset(request, *dst, *index);
          }, {});
        }
        //At this point the source table is empty
      }
    });

    builder.submitTask(std::move(task.setName("asset start")));
  }

  bool moveSucceededAsset(RuntimeDatabase& db, LoadingAsset& asset, size_t assetIndex, const TableID& sourceTable) {
    AssetOperations ops = std::visit(GetAssetOperations{}, *asset.asset);
    if(ops.destinationRow.type == DBTypeID{}) {
      return false;
    }

    RuntimeTable* source = db.tryGet(sourceTable);
    //TODO: could create a map to avoid linear search here
    QueryResult<> query = db.queryAliasTables({ ops.destinationRow });
    assert(query.size() && "A destination for assets should always exist");
    const TableID destTable = query[0];
    RuntimeTable* dest = db.tryGet(destTable);
    assert(dest && "Table must exist since it was found in the query");

    //The entry for the row containing this value will be destroyed by migration, hold onto it to manually place in destination
    std::shared_ptr<AssetVariant> toMove = std::move(asset.asset);
    const size_t dstIndex = RuntimeTable::migrateOne(assetIndex, *source, *dest);
    RuntimeRow* destinationRow = dest->tryGetRow(ops.destinationRow.type);
    assert(destinationRow);
    ops.writeToDestination(*destinationRow, std::move(*toMove), dstIndex);

    return true;
  }

  void moveFailedAsset(RuntimeDatabase& db, size_t assetIndex, const TableID& sourceTable, RuntimeTable& failedTable) {
    RuntimeTable* source = db.tryGet(sourceTable);
    RuntimeTable::migrateOne(assetIndex, *source, failedTable);
  }

  Globals* getGlobals(RuntimeDatabaseTaskBuilder& task) {
    return task.query<GlobalsRow>().tryGetSingletonElement<0>();
  }

  //Look through loading assets and see if their tasks are complete. If so, either moves them to the success or failure tables
  void updateRequestProgress(IAppBuilder& builder) {
    auto task = builder.createTask();
    RuntimeDatabase& db = task.getDatabase();
    auto query = task.query<LoadingAssetRow>();
    QueryResult<> failedRows = task.queryTables<FailedTagRow>();
    assert(failedRows.size());
    RuntimeTable* failedTable = db.tryGet(failedRows[0]);
    Globals* globals = getGlobals(task);
    assert(globals);

    task.setCallback([query, &db, failedTable, globals](AppTaskArgs&) mutable {
      if(!globals->assetCompletionLimit.tryUpdate()) {
        return;
      }
      for(size_t t = 0; t < query.size(); ++t) {
        auto [assets] = query.get(t);
        const TableID thisTable = query.matchingTableIDs[t];
        for(size_t i = 0; i < assets->size();) {
          LoadingAsset& asset = assets->at(i);
          //If it's still in progress check again later
          if(asset.task && !asset.task->isDone()) {
            ++i;
            continue;
          }

          //If it succeeded, move it over to the succeeded table
          if(asset.asset && moveSucceededAsset(db, asset, i, thisTable)) {
            continue;
          }
          //Any other status presumably means some kind of failure, move it to the failed table
          moveFailedAsset(db, i, thisTable, *failedTable);
        }
      }
    });

    builder.submitTask(std::move(task.setName("update asset requests")));
  }

  //Look at all UsageTrackerBlockRows for expired tracker blocks
  void garbageCollectAssets(IAppBuilder& builder) {
    auto task = builder.createTask();
    auto q = task.query<const UsageTrackerBlockRow>();
    auto modifiers = task.getModifiersForTables(q.matchingTableIDs);
    Globals* globals = getGlobals(task);

    task.setCallback([q, modifiers, globals](AppTaskArgs&) mutable {
      if(!globals->assetGCLimit.tryUpdate()) {
        return;
      }
      for(size_t t = 0; t < q.size(); ++t) {
        auto [usages] = q.get(t);
        const auto& modifier = modifiers[t];
        for(size_t i = 0; i < usages->size();) {
          if(usages->at(i).tracker.expired()) {
            modifier->swapRemove(q.matchingTableIDs[t].remakeElement(i));
          }
          else {
            ++i;
          }
        }
      }
    });

    builder.submitTask(std::move(task.setName("gc assets")));
  }

  void processRequests(IAppBuilder& builder) {
    startRequests(builder);
    updateRequestProgress(builder);
    garbageCollectAssets(builder);
  }
}