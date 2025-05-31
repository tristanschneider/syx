#include "Precompile.h"
#include "SweepNPruneBroadphase.h"

#include "Physics.h"
#include "Profile.h"
#include "AppBuilder.h"
#include "SpatialPairsStorage.h"
#include "shapes/ShapeRegistry.h"
#include "Events.h"
#include "TLSTaskImpl.h"

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

  void updateBroadphase(IAppBuilder& builder, const BoundariesConfig&, const PhysicsAliases&) {
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
      void init(const PhysicsAliases& a, const BoundariesConfig& c) {
        physicsAliases = a;
        config = c;
      }

      void init(RuntimeDatabaseTaskBuilder& task) {
        query = task.queryAlias(
            QueryAlias<Events::EventsRow>::create().read(),
            QueryAlias<StableIDRow>::create().read(),
            physicsAliases.posX.read(), physicsAliases.posY.read(),
            QueryAlias<BroadphaseKeys>::create()
          );
        res = task.getAliasResolver(physicsAliases.isImmobile.read());
        ids = task.getRefResolver();
        spatialPairs = SP::createStorageModifier(task);
        grid = task.query<SharedRow<Broadphase::SweepGrid::Grid>>().tryGetSingletonElement();
      }

      PhysicsAliases physicsAliases;
      BoundariesConfig config;
      QueryResult<
        const Events::EventsRow,
        const StableIDRow,
        const Row<float>, const Row<float>,
        BroadphaseKeys
      > query;
      std::shared_ptr<ITableResolver> res;
      Broadphase::SweepGrid::Grid* grid{};
      std::shared_ptr<SP::IStorageModifier> spatialPairs;
      ElementRefResolver ids;
    };

    void init() {}

    void execute(Group& group) {
      CachedRow<const TagRow> isImmobile;
      for(size_t t = 0; t < group.query.size(); ++t) {
        auto [events, stable, posX, posY, keys] = group.query.get(t);
        //If the table it moved to (this table) is immobile, mark as immoble, otherwise do the opposite
        const bool isTableImmobile = group.res->tryGetOrSwapRow(isImmobile, group.query.getTableID(t));
        for(auto event : *events) {
          const size_t i = event.first;
          const ElementRef& self = stable->at(i);
          if(event.second.isMove()) {
            if(isTableImmobile) {
              //Final update at last position before marking as immobile
              const float halfSize = group.config.mHalfSize + group.config.mPadding;
              glm::vec2 min{ posX->at(i) - halfSize, posY->at(i) - halfSize };
              glm::vec2 max{ posX->at(i) + halfSize, posY->at(i) + halfSize };
              Broadphase::BroadphaseKey& key = keys->at(i);
              Broadphase::SweepGrid::updateBoundaries(*group.grid, &min.x, &max.x, &min.y, &max.y, &key, 1);

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

  void postProcessEvents(IAppBuilder& builder, const PhysicsAliases& aliases, const BoundariesConfig& cfg) {
    builder.submitTask(TLSTask::createWithArgs<ProcessImmobileMigrations, ProcessImmobileMigrations::Group>("physics post events", aliases, cfg));
  }
}