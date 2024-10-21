#include "Precompile.h"

#include "AppBuilder.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "loader/AssetLoader.h"
#include "AssetTables.h"
#include "ILocalScheduler.h"

namespace Loader {
  struct ExampleAsset {};
  struct LoadFailure {};
  using AssetVariant = std::variant<
    std::monostate,
    LoadFailure,
    ExampleAsset
  >;

  struct LoadingAsset {
    std::shared_ptr<AssetVariant> asset;
    std::shared_ptr<Tasks::ILongTask> task;
    LoadState state;
  };
  struct LoadingAssetRow : Row<LoadingAsset> {};

  namespace db {
    struct ExampleAssetRow : Row<ExampleAsset> {};
    using LoadingAssetTable = Table<
      LoadingTagRow,
      UsageTrackerBlockRow,
      LoadingAssetRow
    >;
    template<class T>
    using SucceededAssetTable = Table<
      SucceededTagRow,
      UsageTrackerBlockRow,
      T
    >;
    using LoaderDB = Database<
      Table<
        RequestedTagRow,
        LoadRequestRow,
        UsageTrackerBlockRow
      >,
      Table<
        FailedTagRow,
        UsageTrackerBlockRow
      >,
      LoadingAssetTable,
      SucceededAssetTable<ExampleAssetRow>
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

  //Queue tasks for all requests in the requests table
  void startRequests(IAppBuilder& builder) {
    auto task = builder.createTask();
    KnownTables tables{ task };
    auto sourceQuery = task.query<const LoadRequestRow>(tables.requests);
    auto destinationQuery = task.query<LoadingAssetRow>(tables.loading);
    RuntimeDatabase& db = task.getDatabase();
    RuntimeTable* loadingTable = db.tryGet(tables.loading);

    task.setCallback([=, &db](AppTaskArgs& args) mutable {
      auto dq = destinationQuery.get<0>(0);
      for(size_t t = 0; t < sourceQuery.size(); ++t) {
        auto [source] = sourceQuery.get(t);
        RuntimeTable* sourceTable = db.tryGet(sourceQuery.matchingTableIDs[t]);
        if(!sourceTable) {
          continue;
        }
        while(source->size()) {
          const LoadRequest request = std::move(source->at(0));
          const size_t newIndex = dq.size();

          //Create the entry in the destination and remove the current
          RuntimeTable::migrateOne(0, *sourceTable, *loadingTable);
          LoadingAsset& newAsset = dq.at(newIndex);
          //Initialize task metadata and start the task
          newAsset.state.step = Loader::LoadStep::Loading;
          newAsset.asset = std::make_shared<AssetVariant>();
          newAsset.task = args.scheduler->queueLongTask([request, dst{newAsset.asset}](AppTaskArgs&) {
            //Load the asset
            //TODO: load here

            //Store the final result to read in udpateRequestProgress
            *dst = LoadFailure{};
          }, {});
        }
        //At this point the source table is empty
      }
    });

    builder.submitTask(std::move(task.setName("asset start")));
  }

  //Look through loading assets and see if their tasks are complete. If so, either moves them to the success or failure tables
  void updateRequestProgress(IAppBuilder&) {
  }

  //Look at all UsageTrackerBlockRows for expired tracker blocks
  void garbageCollectAssets(IAppBuilder&) {
  }

  void processRequests(IAppBuilder& builder) {
    startRequests(builder);
    updateRequestProgress(builder);
    garbageCollectAssets(builder);
  }
}