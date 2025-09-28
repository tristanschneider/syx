#include "Precompile.h"
#include "SweepNPruneBroadphase.h"

#include "Physics.h"
#include "Profile.h"
#include "AppBuilder.h"
#include "SpatialPairsStorage.h"
#include "shapes/ShapeRegistry.h"
#include "Events.h"
#include "TLSTaskImpl.h"
#include <module/MassModule.h>

namespace SweepNPruneBroadphase {
  void registryUpdate(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("assignBoundaries");
    struct TaskData {
      const SweepNPruneBroadphase::BroadphaseKeys* keys{};
      ShapeRegistry::BroadphaseBounds bounds;
    };
    const auto keyAlias = QueryAlias<SweepNPruneBroadphase::BroadphaseKeys>::create().read();
    using GridQ = SharedRow<Broadphase::SweepGrid::Grid>;
    const auto gridAlias = QueryAlias<const GridQ>::create();

    //Make each shape impl submit their bounds to TaskData which will run before the final assignment task
    auto taskDatas = std::make_shared<std::vector<TaskData>>();
    const auto& lookup = ShapeRegistry::get(task)->lookup();
    taskDatas->reserve(lookup.size());
    for(auto&& [table, impl] : lookup) {
      auto kq = task.queryAlias(table, keyAlias);
      if(!kq.size()) {
        continue;
      }
      TaskData& data = taskDatas->emplace_back();
      data.keys = &kq.get<0>(0);
      data.bounds.table = table;
      data.bounds.requiredDependency = QueryAliasBase{ gridAlias };

      impl->writeBoundaries(builder, data.bounds);
    }

    //After all bounds have been stored in task data, collect them and put them in the broadphase
    auto& grid = *task.query<GridQ>().tryGetSingletonElement();
    task.setCallback([&grid, taskDatas](AppTaskArgs&) mutable {
      for(size_t i = 0; i < taskDatas->size(); ++i) {
        Broadphase::SweepGrid::updateBoundaries(grid,
          taskDatas->at(i).bounds.minX.data(),
          taskDatas->at(i).bounds.maxX.data(),
          taskDatas->at(i).bounds.minY.data(),
          taskDatas->at(i).bounds.maxY.data(),
          taskDatas->at(i).keys->data(),
          taskDatas->at(i).bounds.minX.size()
        );
      }
    });
    builder.submitTask(std::move(task));
  }

  void updateBroadphase(IAppBuilder& builder) {
    registryUpdate(builder);
    Broadphase::SweepGrid::recomputePairs(builder);
    SP::updateSpatialPairsFromBroadphase(builder);
  }

  struct AddAndRemoveFromBroadphase {
    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
      ids = task.getRefResolver();
      spatialPairs = SP::createStorageModifier(task);
      grid = task.query<SharedRow<Broadphase::SweepGrid::Grid>>().tryGetSingletonElement();
    }

    void execute() {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [events, stables, keys] = query.get(t);
        for(auto event : *events) {
          const size_t i = event.first;
          //Insert new elements.
          if(event.second.isCreate()) {
            //If they were created and destroyed on the same frame don't bother creating the key
            if(event.second.isDestroy()) {
              continue;
            }
            Broadphase::BroadphaseKey& key = keys->at(i);
            const Broadphase::UserKey userKey = stables->at(i);
            Broadphase::SweepGrid::insertRange(*grid, &userKey, &key, 1);
            spatialPairs->addSpatialNode(userKey, false);
          }
          else if(event.second.isDestroy()) {
            //Remove elements that are about to be destroyed
            Broadphase::BroadphaseKey& key = keys->at(i);
            Broadphase::SweepGrid::eraseRange(*grid, &key, 1);
            spatialPairs->removeSpatialNode(stables->at(i));
            key = {};
          }
        }
      }
    }

    QueryResult<const Events::EventsRow,
      const StableIDRow,
      BroadphaseKeys
    > query;
    ElementRefResolver ids;
    std::shared_ptr<SP::IStorageModifier> spatialPairs;
    Broadphase::SweepGrid::Grid* grid{};
  };

  void preProcessEvents(IAppBuilder& builder) {
    builder.submitTask(TLSTask::create<AddAndRemoveFromBroadphase>("physics pre events"));
  }

  struct ProcessImmobileMigrations {
    struct Group {
      void init(RuntimeDatabaseTaskBuilder& task) {
        query = task;
        res = task.getResolver<const MassModule::IsImmobile>();
        ids = task.getRefResolver();
        spatialPairs = SP::createStorageModifier(task);
        grid = task.query<SharedRow<Broadphase::SweepGrid::Grid>>().tryGetSingletonElement();
      }

      QueryResult<
        const Events::EventsRow,
        const StableIDRow,
        const BroadphaseKeys
      > query;
      std::shared_ptr<ITableResolver> res;
      Broadphase::SweepGrid::Grid* grid{};
      std::shared_ptr<SP::IStorageModifier> spatialPairs;
      ElementRefResolver ids;
    };

    void init() {}

    void execute(Group& group) {
      CachedRow<const MassModule::IsImmobile> isImmobile;
      for(size_t t = 0; t < group.query.size(); ++t) {
        auto [events, stable, keys] = group.query.get(t);
        //If the table it moved to (this table) is immobile, mark as immoble, otherwise do the opposite
        const bool isTableImmobile = group.res->tryGetOrSwapRow(isImmobile, group.query.getTableID(t));
        for(auto event : *events) {
          const size_t i = event.first;
          const ElementRef& self = stable->at(i);
          if(event.second.isMove()) {
            if(isTableImmobile) {
              //Change node to immobile
              group.spatialPairs->changeMobility(self, true);
            }
            else {
              //Change node to mobile
              group.spatialPairs->changeMobility(self, false);
            }
          }
        }
      }
    }
  };

  void postProcessEvents(IAppBuilder& builder) {
    builder.submitTask(TLSTask::create<ProcessImmobileMigrations, ProcessImmobileMigrations::Group>("physics post events"));
  }
}