#include "Precompile.h"
#include "loader/AssetService.h"

#include "AppBuilder.h"
#include "Events.h"
#include "loader/AssetLoader.h"
#include "loader/SceneAsset.h"
#include "AssetTables.h"
#include "ILocalScheduler.h"
#include "glm/glm.hpp"
#include "generics/Hash.h"
#include "generics/RateLimiter.h"
#include "AssimpImporter.h"
#include "AssetLoadTask.h"
#include "AssetIndex.h"
#include "AssetDatabase.h"
#include "MaterialImporter.h"
#include "IAssetImporter.h"

namespace Loader {
  std::string_view getExtension(const std::string& str) {
    const size_t last = str.find_last_of('.');
    return last == str.npos ? std::string_view{} : std::string_view{ str.begin() + last + 1, str.end() };
  }

  void loadAsset(const LoadRequest& request, AppTaskArgs& taskArgs, AssetLoadTask& task) {
    *task.asset = LoadFailure{};
    std::array importers{
      Loader::createAssimpImporter(task, taskArgs),
      Loader::createMaterialImporter(MaterialImportSampleMode::GuessFromSize)
    };

    const std::string_view ext{ getExtension(request.location.filename) };
    for(auto& importer : importers) {
      if(importer->isSupportedExtension(ext)) {
        importer->loadAsset(request, task.asset);
        return;
      }
    }
    printf("No loader implemented for request [%s]\n", request.location.filename.c_str());
  }

  //Queue tasks for all requests in the requests table
  void startRequests(IAppBuilder& builder) {
    auto task = builder.createTask();
    KnownTables tables{ task };
    auto sourceQuery = task.query<const LoadRequestRow, const StableIDRow, const UsageTrackerBlockRow>(tables.requests);
    auto destinationQuery = task.query<LoadingAssetRow>(tables.loading);
    const AssetIndex* index = task.query<const AssetIndexRow>().tryGetSingletonElement();
    assert(index);
    RuntimeDatabase& db = task.getDatabase();
    RuntimeTable* loadingTable = db.tryGet(tables.loading);

    task.setCallback([=, &db](AppTaskArgs& args) mutable {
      auto& dq = destinationQuery.get<0>(0);
      StableElementMappings& mappings = db.getMappings();
      for(size_t t = 0; t < sourceQuery.size(); ++t) {
        auto [source, stable, usage] = sourceQuery.get(t);
        RuntimeTable* sourceTable = db.tryGet(sourceQuery[t]);
        if(!sourceTable) {
          continue;
        }
        while(source->size()) {
          const LoadRequest request = source->at(0);
          const size_t newIndex = dq.size();

          //Get the handle to this element being moved to the loading table
          //This refers to the primary asset. Additional assets from subtasks can create their own AssetHandles referring to new locations that
          //won't exist until updateRequestProgress finalizes the asset load
          AssetHandle self{
            .asset = stable->at(0),
            .use = usage->at(0).tracker.lock(),
          };
          assert(self.use && "Tracker should be alive during asset load");

          //Create the entry in the destination and remove the current
          RuntimeTable::migrate(0, *sourceTable, *loadingTable, 1);
          LoadingAsset& newAsset = dq.at(newIndex);
          //Initialize task metadata and start the task
          newAsset.state.step = Loader::LoadStep::Loading;
          const AssetLoadTaskDeps deps{
            .mappings = mappings,
            .index = *index
          };
          const AssetLoadTaskArgs taskArgs{
            .self = self,
            .deps = deps,
            .hasPendingHandle = false
          };
          AssetLoadTask::addTask(newAsset.task, args, taskArgs, [request](AppTaskArgs& args, AssetLoadTask& self) {
            loadAsset(request, args, self);
          });
        }
        //At this point the source table is empty
      }
    });

    builder.submitTask(std::move(task.setName("asset start")));
  }

  void moveSucceededAssets(
    RuntimeDatabase& db,
    std::vector<std::pair<AssetLoadTask*, AssetOperations>>& assets,
    ElementRefResolver& res,
    const TableID& sourceTable
  ) {
    for(auto&& [task, ops] : assets) {
      //Types that don't count as failures but also don't have a destination are skipped
      if(ops.destinationRow.type == DBTypeID{}) {
        continue;
      }
      RuntimeTable* source = db.tryGet(sourceTable);
      //TODO: could create a map to avoid linear search here
      QueryResultBase query = db.queryAliasTables({ ops.destinationRow });
      assert(query.size() && "A destination for assets should always exist");
      const TableID destTable = query[0];
      RuntimeTable* dest = db.tryGet(destTable);
      assert(dest && "Table must exist since it was found in the query");

      //The entry for the row containing this value will be destroyed by migration, hold onto it to manually place in destination
      AssetVariant toMove = std::move(task->asset);
      size_t dstIndex{};
      if(task->hasStorage()) {
        //For the element that already has storage, move it from source to destination table
        auto id = res.tryUnpack(task->taskArgs.self.asset);
        assert(id && id->getTableIndex() == sourceTable.getTableIndex() && "All tasks are expected to come from the same table");

        dstIndex = RuntimeTable::migrate(id->getElementIndex(), *source, *dest, 1);
      }
      else {
        //For elements that were pending, create their entries now
        dstIndex = dest->addElements(1, &task->taskArgs.self.asset);

        //Now that the entry for this ElementRef is being created, mark it as claimed
        task->setHandleClaimed();

        //Move the pending usage block over to the destination
        if(auto usage = dest->tryGet<UsageTrackerBlockRow>()) {
          usage->at(dstIndex).tracker = task->taskArgs.self.use;
        }
      }

      //Broadcast notification of this
      //TODO: use local database to do this
      if(auto events = dest->tryGet<Events::EventsRow>()) {
        events->getOrAdd(dstIndex).setCreate();
      }

      //Move the asset itself to the destination
      IRow* destinationRow = dest->tryGet(ops.destinationRow.type);
      assert(destinationRow);
      ops.writeToDestination(*destinationRow, std::move(toMove), dstIndex);
    }
  }

  void moveFailedAssets(
    RuntimeDatabase& db,
    std::vector<std::pair<AssetLoadTask*, AssetOperations>>& assets,
    ElementRefResolver& res,
    const TableID& sourceTable,
    RuntimeTable& failedTable
  ) {
    RuntimeTable* source = db.tryGet(sourceTable);
    for(auto&& [task, ops] : assets) {
      //If it had storage, move it to the failed table
      //If it didn't, the pending ElementRefs will be reclaimed by the task destructor
      if(task->hasStorage()) {
        auto id = res.tryUnpack(task->taskArgs.self.asset);
        assert(id && id->getTableIndex() == sourceTable.getTableIndex() && "All tasks are expected to come from the same table");
        RuntimeTable::migrate(id->getElementIndex(), *source, failedTable, 1);
      }
    }
  }

  Globals* getGlobals(RuntimeDatabaseTaskBuilder& task) {
    return task.query<GlobalsRow>().tryGetSingletonElement<0>();
  }

  //Look through loading assets and see if their tasks are complete. If so, either moves them to the success or failure tables
  void updateRequestProgress(IAppBuilder& builder) {
    auto task = builder.createTask();
    RuntimeDatabase& db = task.getDatabase();
    auto query = task.query<LoadingAssetRow>();
    QueryResultBase failedRows = task.queryTables<FailedTagRow>();
    assert(failedRows.size());
    RuntimeTable* failedTable = db.tryGet(failedRows[0]);
    Globals* globals = getGlobals(task);
    assert(globals);
    auto res = task.getIDResolver()->getRefResolver();

    task.setCallback([query, &db, failedTable, globals, res](AppTaskArgs&) mutable {
      if(!globals->assetCompletionLimit.tryUpdate()) {
        return;
      }
      for(size_t t = 0; t < query.size(); ++t) {
        auto [assets] = query.get(t);
        const TableID thisTable = query[t];
        for(size_t i = 0; i < assets->size();) {
          LoadingAsset& asset = assets->at(i);
          //If it's still in progress check again later
          if(asset.task && !asset.task->isDone()) {
            ++i;
            continue;
          }

          //Gather the linked list of assets and see if any failed, which invalidates the entire batch
          std::vector<std::pair<AssetLoadTask*, AssetOperations>> operations;
          AssetLoadTask* task = asset.task.get();
          bool allSucceeded = true;
          while(task) {
            operations.push_back(std::make_pair(task, Loader::getAssetOperations(task->asset)));
            if(operations.back().second.isFailure) {
              //Mark as failed and continue gathering the rest of the list as they still need to be erased
              allSucceeded = false;
            }
            task = task->next.get();
          }

          //If all succeeded, move them over to the succeeded table
          if(allSucceeded) {
            moveSucceededAssets(db, operations, res, thisTable);
          }
          else {
            moveFailedAssets(db, operations, res, thisTable, *failedTable);
          }
        }
      }
    });

    builder.submitTask(std::move(task.setName("update asset requests")));
  }

  //Look at all UsageTrackerBlockRows for expired tracker blocks
  void garbageCollectAssets(IAppBuilder& builder) {
    auto task = builder.createTask();
    auto q = task.query<Events::EventsRow, const UsageTrackerBlockRow>();
    Globals* globals = getGlobals(task);

    task.setCallback([q, globals](AppTaskArgs&) mutable {
      if(!globals->assetGCLimit.tryUpdate()) {
        return;
      }
      for(size_t t = 0; t < q.size(); ++t) {
        auto [events, usages] = q.get(t);
        for(size_t i = 0; i < usages->size(); ++i) {
          if(usages->at(i).tracker.expired()) {
            events->getOrAdd(i).setDestroy();
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

  void createDB(RuntimeDatabaseArgs& args) {
    return Loader::createAssetDatabase(args);
  }
}