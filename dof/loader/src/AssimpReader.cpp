#include "Precompile.h"
#include "AssimpReader.h"

#include "AppBuilder.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "MeshRemapper.h"
#include "loader/SceneAsset.h"
#include "glm/glm.hpp"
#include "STBInterface.h"
#include "AssetLoadTask.h"
#include "AssetTables.h"

namespace Loader {
  struct NodeTraversal {
    const aiNode* node{};
    aiMatrix4x4 transform;
    size_t tableHash{};
  };

  struct SceneLoadContext {
    Assimp::Importer importer;
    AssetLoadTask& task;
    AppTaskArgs args;
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
          constexpr int CHANNELS = 4;
          //If height is not provided it means the texture is in its raw format, use STB to parse that
          if(!tex.first->mHeight) {
            if(ImageData data = STBInterface::loadImageFromBuffer((const unsigned char*)tex.first->pcData, tex.first->mWidth, CHANNELS); data.mBytes) {
              TextureAsset& t = result.materials[i].texture;
              t.format = TextureFormat::RGBA;
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
            t.format = TextureFormat::RGBA;
            t.width = tex.first->mWidth;
            t.height = tex.first->mHeight;
            t.buffer.resize(t.width*t.height*CHANNELS);
            //aiTexel always contains 4 components, target format is 4
            std::memcpy(t.buffer.data(), tex.first->pcData, t.buffer.size());
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

      verts.vertices.resize(mesh->mNumFaces * 3);
      uvs.textureCoordinates.resize(verts.vertices.size());
      //Texture coordinates can be in any slot. Assume if they are provided they are in the first slot
      constexpr int EXPECTED_TEXTURE_CHANNEL = 0;
      const bool hasTexture = mesh->HasTextureCoords(EXPECTED_TEXTURE_CHANNEL);

      //Throw out the third dimension while copying over since assets are 2D, assume Z is unused, coordinates match the game where z is into the screen
      for(unsigned v = 0; v < mesh->mNumFaces; ++v) {
        const aiFace& face = mesh->mFaces[v];
        assert(face.mNumIndices == 3);
        const size_t base = v*3;
        for(unsigned fi = 0; fi < std::min(3u, face.mNumIndices); ++fi) {
          const unsigned vi = face.mIndices[fi];
          const aiVector3D& sourceVert = mesh->mVertices[vi];
          const aiVector3D& sourceUV = mesh->mTextureCoords[EXPECTED_TEXTURE_CHANNEL][vi];
          verts.vertices[base + fi] = glm::vec2{ sourceVert.x, sourceVert.y };
          if(hasTexture) {
            uvs.textureCoordinates[base + fi] = glm::vec2{ sourceUV.x, sourceUV.y };
          }
        }
      }
    }
  }

  void loadSceneAsset(const aiScene& scene, SceneLoadContext& ctx, SceneAsset& result) {
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

  class AssimpReaderImpl : public IAssimpReader {
  public:
    AssimpReaderImpl(AssetLoadTask& task, const AppTaskArgs& taskArgs)
      : ctx{ .task = task, .args = taskArgs }
    {
    }

    bool isSceneExtension(std::string_view extension) final {
      return ctx.importer.IsExtensionSupported(std::string{ extension });
    }

    void loadScene(const Loader::LoadRequest& request) final {
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

  std::unique_ptr<IAssimpReader> createAssimpReader(AssetLoadTask& task, const AppTaskArgs& taskArgs) {
    return std::make_unique<AssimpReaderImpl>(task, taskArgs);
  }
}