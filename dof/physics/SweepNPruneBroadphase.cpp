#include "Precompile.h"
#include "SweepNPruneBroadphase.h"

#include "Physics.h"
#include "Profile.h"
#include "AppBuilder.h"
#include "SpatialPairsStorage.h"
#include "shapes/ShapeRegistry.h"

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
      TaskData& data = taskDatas->emplace_back();
      auto kq = task.queryAlias(table, keyAlias);
      if(!kq.size()) {
        continue;
      }
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

  void preProcessEvents(RuntimeDatabaseTaskBuilder& task, const DBEvents& events) {
    task.setName("physics pre events");
    auto resolver = task.getResolver<
      BroadphaseKeys,
      const StableIDRow
    >();
    auto ids = task.getIDResolver();
    auto spatialPairs = SP::createStorageModifier(task);
    Broadphase::SweepGrid::Grid& grid = *task.query<SharedRow<Broadphase::SweepGrid::Grid>>().tryGetSingletonElement();

    task.setCallback([resolver, ids, &grid, &events, spatialPairs](AppTaskArgs&) mutable {
      CachedRow<BroadphaseKeys> keys;
      CachedRow<const StableIDRow> stable;
      for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
        //Insert new elements
        if(cmd.isCreate()) {
          const auto unpacked = ids->uncheckedUnpack(cmd.destination);
          if(resolver->tryGetOrSwapAllRows(unpacked, keys, stable)) {
            Broadphase::BroadphaseKey& key = keys->at(unpacked.getElementIndex());
            const Broadphase::UserKey userKey = stable->at(unpacked.getElementIndex());
            Broadphase::SweepGrid::insertRange(grid, &userKey, &key, 1);
            spatialPairs->addSpatialNode(cmd.destination, false);
          }
        }
        else if(cmd.isDestroy()) {
          //Remove elements that are about to be destroyed
          const auto unpacked = ids->uncheckedUnpack(cmd.source);
          if(resolver->tryGetOrSwapRow(keys, unpacked)) {
            Broadphase::BroadphaseKey& key = keys->at(unpacked.getElementIndex());
            Broadphase::SweepGrid::eraseRange(grid, &key, 1);
            spatialPairs->removeSpatialNode(cmd.source);
            key = {};
          }
        }
      }
    });
  }

  void postProcessEvents(RuntimeDatabaseTaskBuilder& task, const DBEvents& events, const PhysicsAliases& aliases, const BoundariesConfig& cfg) {
    task.setName("physics post events");
    Broadphase::SweepGrid::Grid& grid = *task.query<SharedRow<Broadphase::SweepGrid::Grid>>().tryGetSingletonElement();
    auto resolver = task.getAliasResolver(
      aliases.posX.read(),
      aliases.posY.read(),
      aliases.isImmobile.read(),
      QueryAlias<BroadphaseKeys>::create()
    );
    auto ids = task.getIDResolver();
    auto spatialPairs = SP::createStorageModifier(task);

    task.setCallback([resolver, &grid, ids, &events, cfg, aliases, spatialPairs](AppTaskArgs&) mutable {
      CachedRow<const Row<float>> posX, posY;
      CachedRow<const TagRow> isImmobile;
      CachedRow<BroadphaseKeys> keys;
      //Bounds update elements that moved to an immobile row
      for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
        std::optional<ResolvedIDs> found = cmd.isCreate() ? ids->tryResolveAndUnpack(cmd.destination) : ids->tryResolveAndUnpack(cmd.source);

        if(found) {
          //The stable mappings are pointing at the raw index, then assume that it ended up at the destination table
          UnpackedDatabaseElementID self{ found->unpacked };
          const UnpackedDatabaseElementID rawDest = ids->uncheckedUnpack(cmd.destination);
          //Should always be the case unless it somehow moved more than once
          if(self.getTableIndex() == rawDest.getTableIndex()) {
            const auto unpacked = self;
            if(resolver->tryGetOrSwapRowAlias(aliases.isImmobile.read(), isImmobile, unpacked)) {
              resolver->tryGetOrSwapRowAlias(aliases.posX.read(), posX, unpacked);
              resolver->tryGetOrSwapRowAlias(aliases.posY.read(), posY, unpacked);
              resolver->tryGetOrSwapRowAlias(QueryAlias<BroadphaseKeys>::create(), keys, unpacked);

              //TODO: this doesn't take ShapeRegistry into account
              if(posX && posY && keys) {
                const float halfSize = cfg.mHalfSize + cfg.mPadding;
                const size_t i = unpacked.getElementIndex();
                glm::vec2 min{ posX->at(i) - halfSize, posY->at(i) - halfSize };
                glm::vec2 max{ posX->at(i) + halfSize, posY->at(i) + halfSize };
                Broadphase::BroadphaseKey& key = keys->at(i);
                Broadphase::SweepGrid::updateBoundaries(grid, &min.x, &max.x, &min.y, &max.y, &key, 1);
              }
              //Change node to immobile
              spatialPairs->changeMobility(found->stable, true);
            }
            else {
              //Change node to mobile
              spatialPairs->changeMobility(found->stable, false);
            }
          }
        }
      }
    });
  }
}