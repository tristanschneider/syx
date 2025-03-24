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
    //Materials and meshes corresponding ot what meshMap produces
    const std::vector<AssetHandle>* resolvedMaterials{};
    const std::vector<AssetHandle>* resolvedMeshes{};
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

  //TODO: assuming axis/angle, is that right?
  float toRot(const aiVector3D& axis, ai_real angle) {
    return axis.z > 0 ? angle : -angle;
  }

  Transform loadTransform(const aiMatrix4x4& data) {
    aiVector3D scale, axis, translate;
    ai_real angle{};
    data.Decompose(scale, axis, angle, translate);
    return Transform{
      .pos = glm::vec3{ translate.x, translate.y, translate.z },
      .scale = glm::vec3{ scale.x, scale.y, scale.z },
      .rot = toRot(axis, angle)
    };
  }

  AssetHandle tryGetIndex(const std::vector<AssetHandle>& container, uint32_t i) {
    return i < container.size() ? container[i] : AssetHandle{};
  }

  std::optional<MatMeshRef> tryLoadMeshIndex(const aiNode& e, const SceneLoadContext& context) {
    if(e.mNumMeshes) {
      const Loader::MeshIndex index = context.meshMap->remap(e.mMeshes[0]);
      return MatMeshRef{
        .material = tryGetIndex(*context.resolvedMaterials, index.materialIndex),
        .mesh = tryGetIndex(*context.resolvedMeshes, index.meshIndex)
      };
    }
    return {};
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

  void loadMaterials(const aiScene& scene, SceneLoadContext& ctx, LoadingSceneAsset& result) {
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

  void loadMeshes(const aiScene& scene, SceneLoadContext& ctx, LoadingSceneAsset& result) {
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

  void gatherModelsAndMaterials(SceneLoadContext& ctx, LoadingSceneAsset& scene, ModelsAndMaterials& result) {
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

  void assignDeduplicatedModelsAndMaterials(LoadingSceneAsset& scene, ModelsAndMaterials& toAssign) {
    scene.materials.resize(toAssign.materials.size());
    std::transform(toAssign.materials.begin(), toAssign.materials.end(), scene.materials.begin(), MeshRemapper::RemapRefUnwrapper{});
  }

  struct NoValue {
    static constexpr std::false_type HAS_VALUE;
  };
  template<IsRow ERow, IsRow SRow>
  struct ValueT {
    using row_type = ERow;
    using shared_row_type = SRow;
    using element_type = typename ERow::ElementT;
    static constexpr std::true_type HAS_VALUE;
    static constexpr bool hasValue() { return true; }

    element_type value{};
  };
  struct BoolValue : ValueT<BoolRow, SharedBoolRow> {};
  struct BitfieldValue : ValueT<BitfieldRow, SharedBitfieldRow> {};
  struct IntValue : ValueT<IntRow, SharedIntRow> {};
  struct FloatValue : ValueT<FloatRow, SharedFloatRow> {};
  struct Vec2Value : ValueT<Vec2Row, SharedVec2Row> {};
  struct Vec3Value : ValueT<Vec3Row, SharedVec3Row> {};
  struct Vec4Value : ValueT<Vec4Row, SharedVec4Row> {};
  struct StringValue : ValueT<StringRow, SharedStringRow> {};

  using SingleElementVariant = std::variant<
    NoValue,
    BoolValue,
    BitfieldValue,
    IntValue,
    FloatValue,
    Vec2Value,
    Vec3Value,
    Vec4Value,
    StringValue
  >;

  std::optional<float> tryReadFloat(const aiMetadataEntry& data) {
    switch(data.mType) {
      case AI_FLOAT: return *static_cast<float*>(data.mData);
      case AI_DOUBLE: return static_cast<float>(*static_cast<double*>(data.mData));
    }
    return {};
  }

  std::optional<bool> tryReadBool(const aiMetadataEntry& data) {
    switch(data.mType) {
      case AI_BOOL: return *static_cast<bool*>(data.mData);
    }
    return {};
  }

  std::optional<int32_t> tryReadInt(const aiMetadataEntry& data) {
    switch(data.mType) {
      case AI_INT32: return *static_cast<int32_t*>(data.mData);
      case AI_UINT64: return static_cast<int32_t>(*static_cast<uint64_t*>(data.mData));
      case AI_INT64: return static_cast<int32_t>(*static_cast<int64_t*>(data.mData));
      case AI_UINT32: return static_cast<int32_t>(*static_cast<uint32_t*>(data.mData));
    }
    return {};
  }

  std::optional<std::string> tryReadString(const aiMetadataEntry& data) {
    switch(data.mType) {
      case AI_AISTRING: return std::string{ static_cast<aiString*>(data.mData)->C_Str() };
    }
    return {};
  }

  std::optional<glm::vec3> tryReadVec3(const aiMetadataEntry& data) {
    switch(data.mType) {
      case AI_AIVECTOR3D: {
        auto* v = static_cast<aiVector3D*>(data.mData);
        return glm::vec3{ v->x, v->y, v->z };
      }
    }
    return {};
  }

  SingleElementVariant readSingleElement(const aiMetadataEntry& data) {
    if(const auto v = tryReadBool(data)) {
      return BoolValue{ *v };
    }
    if(const auto v = tryReadInt(data)) {
      return IntValue{ *v };
    }
    if(const auto v = tryReadFloat(data)) {
      return FloatValue{ *v };
    }
    if(const auto v = tryReadString(data)) {
      return StringValue{ *v };
    }
    if(const auto v = tryReadVec3(data)) {
      return Vec3Value{ *v };
    }
    if(data.mType != AI_AIMETADATA) {
      return NoValue{};
    }

    //Metadata could be bitfield or one of the float types, figure out which based on size and type
    const auto* meta = static_cast<const aiMetadata*>(data.mData);
    const size_t count = static_cast<size_t>(meta->mNumProperties);
    if(!count) {
      return NoValue{};
    }
    //If the first value is a float, try to read all values as a float
    //Fail if any are the wrong type
    //Interpret as float if it's an array of one, otherwise vec2,3,4. Bigger than that is not supported.
    if(tryReadFloat(meta->mValues[0])) {
      if(count > 4) {
        return NoValue{};
      }
      std::array<float, 4> values{};
      for(int i = 0; i < count; ++i) {
        if(auto v = tryReadFloat(meta->mValues[i])) {
          values[i] = *v;
        }
        else {
          return NoValue{};
        }
      }

      if(count == 1) {
        return FloatValue{ values[0] };
      }
      if(count == 2) {
        return Vec2Value{ glm::vec2{ values[0], values[1] } };
      }
      if(count == 3) {
        return Vec3Value{ glm::vec3{ values[0], values[1], values[2] } };
      }
      if(count == 4) {
        return Vec4Value{ glm::vec4{ values[0], values[1], values[2], values[3] } };
      }
      //Unreachable
      return NoValue{};
    }
    //Interpret as a bitfield up to 64 size
    else if(tryReadBool(meta->mValues[0])) {
      uint64_t result{};
      uint64_t currentMask = 1;
      const size_t bitfieldSize = std::min(size_t(64), count);
      for(unsigned i = 0; i < bitfieldSize; ++i) {
        if(auto v = tryReadBool(meta->mValues[i])) {
          if(*v) {
            result |= currentMask;
          }
        }
        //Exit if the bool failed to read (invalid type, not false)
        else {
          return NoValue{};
        }

        currentMask = currentMask << 1;
      }

      return BitfieldValue{ result };
    }

    return NoValue{};
  }


  template<IsRow T>
  struct DynamicRowStorage : ChainedRuntimeStorage {
    using SelfT = DynamicRowStorage<T>;
    using ChainedRuntimeStorage::ChainedRuntimeStorage;

    static T& addRowToChain(DBTypeID key, RuntimeTableRowBuilder& table, RuntimeDatabaseArgs& storage) {
      SelfT* self = RuntimeStorage::addToChain<SelfT>(storage);
      table.rows.push_back(RuntimeTableRowBuilder::Row{
        .type = key,
        .row = &self->row
      });
      return self->row;
    }
    T row;
  };

  //TODO: do I need the string versions?
  template<IsRow T>
  T& getOrCreateRow(const std::string_view rowName, RuntimeTableRowBuilder& table, LoadingSceneAsset& scene) {
    return getOrCreateRow<T>(gnx::Hash::constHash(rowName), table, scene);
  }

  RuntimeTableRowBuilder& getOrCreateTable(size_t hash, LoadingSceneAsset& scene) {
    RuntimeDatabaseArgs& storage = scene.loadingArgs;

    if(auto it = std::find_if(storage.tables.begin(), storage.tables.end(), [&hash](const RuntimeTableRowBuilder& t) { return t.tableType.value == hash; }); it != storage.tables.end()) {
      return *it;
    }

    storage.tables.push_back(RuntimeTableRowBuilder{ .tableType = hash });
    RuntimeTableRowBuilder& result = storage.tables.back();
    return result;
  }

  RuntimeTableRowBuilder& getOrCreateTable(const std::string_view tableName, LoadingSceneAsset& scene) {
    return getOrCreateTable(gnx::Hash::constHash(tableName), scene);
  }

  template<IsRow T>
  T& getOrCreateRow(size_t rowName, RuntimeTableRowBuilder& table, LoadingSceneAsset& scene) {
    RuntimeDatabaseArgs& storage = scene.loadingArgs;
    const DBTypeID key = getDynamicRowKey<T>(rowName);

    if(auto it = std::find_if(table.rows.begin(), table.rows.end(), [&key](const auto& row) { return row.type == key; }); it != table.rows.end()) {
      return static_cast<T&>(*it->row);
    }

    return DynamicRowStorage<T>::addRowToChain(key, table, storage);
  }

  struct TableInfo {
    size_t size{};
  };
  struct TableInfoRow : SharedRow<TableInfo> {
    static constexpr std::string_view KEY = "__info__";
  };

  TableInfo& getTableInfo(RuntimeTableRowBuilder& table, LoadingSceneAsset& scene) {
    return getOrCreateRow<TableInfoRow>(TableInfoRow::KEY, table, scene).at();
  }

  template<class T>
  void assignElement(BasicRow<T>& row, size_t i, T&& value) {
    if(i >= row.size()) {
      row.resize(row.size(), i + 1);
    }
    row.at(i) = std::move(value);
  }

  //Read all userdata keys under a table as individual row elements in that table
  //Transforms are always read
  void loadObject(size_t tableName, const NodeTraversal& node, SceneLoadContext& ctx, LoadingSceneAsset& scene) {
    RuntimeTableRowBuilder& table = getOrCreateTable(tableName, scene);
    TableInfo& info = getTableInfo(table, scene);
    const size_t i = info.size++;

    TransformRow& transform = getOrCreateRow<TransformRow>(TransformRow::KEY, table, scene);
    assignElement(transform, i, loadTransform(node.transform));

    if(auto mesh = tryLoadMeshIndex(*node.node, ctx)) {
      assignElement(getOrCreateRow<MatMeshRefRow>(MatMeshRefRow::KEY, table, scene), i, std::move(*mesh));
    }

    readMetadata(*node.node, ctx, [&](size_t hash, const aiMetadataEntry& data, SceneLoadContext&) {
      SingleElementVariant element = readSingleElement(data);
      std::visit([&](auto& v) {
        using This = std::decay_t<decltype(v)>;
        if constexpr(This::HAS_VALUE) {
          using T = typename This::row_type;
          using E = typename This::element_type;
          T& thisRow = getOrCreateRow<T>(hash, table, scene);
          assignElement(thisRow, i, std::move(v.value));
        }
      }, element);
    });
  }

  //Read all userdata keys of a table as SharedRows of the table
  //Nested table userdata will be ignored
  void loadTable(std::string_view tableName, const NodeTraversal& node, SceneLoadContext& ctx, LoadingSceneAsset& scene) {
    RuntimeTableRowBuilder& table = getOrCreateTable(node.tableHash, scene);
    if(table.rows.empty()) {
      getOrCreateRow<TableNameRow>(TableNameRow::KEY, table, scene).at().name = tableName;
    }
    readMetadata(*node.node, ctx, [&](size_t hash, const aiMetadataEntry& data, SceneLoadContext&) {
      const SingleElementVariant element = readSingleElement(data);
      std::visit([&](auto& v) {
        using This = std::decay_t<decltype(v)>;
        if constexpr(This::HAS_VALUE) {
          using T = typename This::shared_row_type;
          T& thisRow = getOrCreateRow<T>(hash, table, scene);
          thisRow.at() = v.value;
        }
      }, element);
    });
  }

  std::unique_ptr<RuntimeDatabase> createSceneDatabase(LoadingSceneAsset& scene) {
    auto result = std::make_unique<RuntimeDatabase>(std::move(scene.loadingArgs));
    //All rows were parsed independently so could be missing elements. Resize them all to consistent table sizes now
    //Doing this after the runtime tables are created ensures the size members on the table itself are also in sync with the pre-filled rows
    for(size_t i = 0 ; i < result->size(); ++i) {
      RuntimeTable& table = (*result)[i];
      if(const TableInfoRow* info = tryGetDynamicRow<TableInfoRow>(table)) {
        table.resize(info->at().size);
      }
    }
    return result;
  }

  void loadSceneAsset(const aiScene& scene, SceneLoadContext& ctx, LoadingSceneAsset& result) {
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

    ctx.resolvedMaterials = &result.materials;
    ctx.resolvedMeshes = &result.meshes;

    ctx.nodesToTraverse.push_back({ scene.mRootNode });
    while(ctx.nodesToTraverse.size()) {
      //Currently ignoring hierarchy, so depth or breadth first doesn't matter
      NodeTraversal node = ctx.nodesToTraverse.back();
      ctx.nodesToTraverse.pop_back();
      if(node.node) {
        //If this isn't a child of a table, try to find the table metadata and parse as table
        if(!node.tableHash) {
          std::string_view tableName;
          readMetadata(*node.node, ctx, [&tableName](size_t hash, const aiMetadataEntry& data, SceneLoadContext&) {
           switch(hash) {
           case gnx::Hash::constHash("Table"):
             if(data.mType == AI_AISTRING) {
               tableName = toView(*static_cast<const aiString*>(data.mData));
             }
             break;
           }
          });

          if(tableName.size()) {
            node.tableHash = gnx::Hash::constHash(tableName);
            loadTable(tableName, node, ctx, result);
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

    result.finalAsset.db = createSceneDatabase(result);
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
        ctx.task.asset.v = LoadingSceneAsset{};
        loadSceneAsset(*scene, ctx, std::get<LoadingSceneAsset>(ctx.task.asset.v));
      }
    }

    SceneLoadContext ctx;
  };

  std::unique_ptr<IAssetImporter> createAssimpImporter(AssetLoadTask& task, const AppTaskArgs& taskArgs) {
    return std::make_unique<AssimpReaderImpl>(task, taskArgs);
  }
}