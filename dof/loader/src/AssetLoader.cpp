#include "Precompile.h"

#include "AppBuilder.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "loader/AssetLoader.h"
#include "loader/AssetBase.h"

namespace Loader {
  namespace db {
    using LoaderDB = Database<
      Table<LoadStateRow>
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

  void processRequests(IAppBuilder& builder) {
    builder;
    //testLoadAsset();
  }
}