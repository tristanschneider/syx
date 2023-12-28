#include "Precompile.h"
#include "SweepNPruneBroadphase.h"

#include "Physics.h"
#include "Profile.h"
#include "AppBuilder.h"
#include "SpatialPairsStorage.h"

namespace SweepNPruneBroadphase {
  void updateUnitCubeBoundaries(IAppBuilder& builder, const BoundariesConfig& cfg, const PhysicsAliases& aliases) {
    const auto keyAlias = QueryAlias<SweepNPruneBroadphase::BroadphaseKeys>::create().read();
    auto tables = builder.queryAliasTables(aliases.posX, aliases.posY, keyAlias);

    struct Temp {
      struct Query {
        std::vector<float> minX, maxX, minY, maxY;
      };
      std::vector<Query> data;
    };
    auto t = std::make_shared<Temp>();
    t->data.resize(tables.size());

    using GridQ = SharedRow<Broadphase::SweepGrid::Grid>;
    for(size_t i = 0; i < tables.size(); ++i) {
      const UnpackedDatabaseElementID& table = tables.matchingTableIDs[i];
      {
        auto task = builder.createTask();
        task.setName("recompute boundary x");
        auto bounds = task.queryAlias(table, aliases.posX);
        //Artificial dependency on broadphase so this comes before the final update
        task.query<const GridQ>();
        task.setCallback([t, bounds, i, cfg](AppTaskArgs&) mutable {
          const float halfSize = cfg.mHalfSize + cfg.mPadding;
          std::vector<float>& min = t->data[i].minX;
          std::vector<float>& max = t->data[i].maxX;
          const auto& posX = bounds.get<0>(0);
          min.resize(posX.size());
          max.resize(posX.size());
          for(size_t c = 0; c < posX.size(); ++c) {
            const float p = posX.at(c);
            min[c] = p - halfSize;
            max[c] = p + halfSize;
          }
        });
        builder.submitTask(std::move(task));
      }

      {
        auto task = builder.createTask();
        task.setName("recompute boundary y");
        auto bounds = task.queryAlias(table, aliases.posY);
        //Artificial dependency on broadphase so this comes before the final update
        task.query<const GridQ>();
        task.setCallback([t, bounds, i, cfg](AppTaskArgs&) mutable {
          const float halfSize = cfg.mHalfSize + cfg.mPadding;
          std::vector<float>& min = t->data[i].minY;
          std::vector<float>& max = t->data[i].maxY;
          const auto& posY = bounds.get<0>(0);
          min.resize(posY.size());
          max.resize(posY.size());
          for(size_t c = 0; c < posY.size(); ++c) {
            const float p = posY.at(c);
            min[c] = p - halfSize;
            max[c] = p + halfSize;
          }
        });
        builder.submitTask(std::move(task));
      }
    }

    auto task = builder.createTask();
    task.setName("assignBoundaries");
    auto& grid = *task.query<GridQ>().tryGetSingletonElement();
    std::vector<const SweepNPruneBroadphase::BroadphaseKeys*> keys(tables.size());
    for(size_t i = 0; i < tables.size(); ++i) {
      keys[i] = &task.queryAlias(tables.matchingTableIDs[i], keyAlias).get<0>(0);
    }
    task.setCallback([&grid, keys, t](AppTaskArgs&) mutable {
      for(size_t i = 0; i < t->data.size(); ++i) {
        Broadphase::SweepGrid::updateBoundaries(grid,
          t->data[i].minX.data(),
          t->data[i].maxX.data(),
          t->data[i].minY.data(),
          t->data[i].maxY.data(),
          keys[i]->data(),
          t->data[i].minX.size()
        );
      }
    });
    builder.submitTask(std::move(task));
  }

  void updateDirectBoundaries(IAppBuilder& builder, const PhysicsAliases& aliases) {
    auto task = builder.createTask();
    task.setName("update broadphase direct");
    Broadphase::SweepGrid::Grid& grid = *task.query<SharedRow<Broadphase::SweepGrid::Grid>>().tryGetSingletonElement();
    auto bounds = task.queryAlias(
      QueryAlias<const SweepNPruneBroadphase::BroadphaseKeys>::create().read(),
      aliases.broadphaseMinX.read(),
      aliases.broadphaseMaxX.read(),
      aliases.broadphaseMinY.read(),
      aliases.broadphaseMaxY.read()
    );

    task.setCallback([&grid, bounds](AppTaskArgs&) mutable {
      for(size_t t = 0; t < bounds.size(); ++t) {
        auto rows = bounds.get(t);
        Broadphase::SweepGrid::updateBoundaries(grid,
          std::get<1>(rows)->data(),
          std::get<2>(rows)->data(),
          std::get<3>(rows)->data(),
          std::get<4>(rows)->data(),
          std::get<0>(rows)->data(),
          std::get<0>(rows)->size()
        );
      }
    });

    builder.submitTask(std::move(task));
  }

  void updateBroadphase(IAppBuilder& builder, const BoundariesConfig& cfg, const PhysicsAliases& aliases) {
    updateDirectBoundaries(builder, aliases);
    updateUnitCubeBoundaries(builder, cfg, aliases);
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
        //TODO: this won't work as expected if the newly created element is immobile
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
        if(auto found = ids->tryResolveStableID(cmd.source)) {
          //The stable mappings are pointing at the raw index, then assume that it ended up at the destination table
          UnpackedDatabaseElementID self{ ids->uncheckedUnpack(*found) };
          const UnpackedDatabaseElementID rawDest = ids->uncheckedUnpack(cmd.destination);
          //Should always be the case unless it somehow moved more than once
          if(self.getTableIndex() == rawDest.getTableIndex()) {
            const auto unpacked = self;
            if(resolver->tryGetOrSwapRowAlias(aliases.isImmobile.read(), isImmobile, unpacked)) {
              resolver->tryGetOrSwapRowAlias(aliases.posX.read(), posX, unpacked);
              resolver->tryGetOrSwapRowAlias(aliases.posY.read(), posY, unpacked);
              resolver->tryGetOrSwapRowAlias(QueryAlias<BroadphaseKeys>::create(), keys, unpacked);

              if(posX && posY && keys) {
                const float halfSize = cfg.mHalfSize + cfg.mPadding;
                const size_t i = unpacked.getElementIndex();
                glm::vec2 min{ posX->at(i) - halfSize, posY->at(i) - halfSize };
                glm::vec2 max{ posX->at(i) + halfSize, posY->at(i) + halfSize };
                Broadphase::BroadphaseKey& key = keys->at(i);
                Broadphase::SweepGrid::updateBoundaries(grid, &min.x, &max.x, &min.y, &max.y, &key, 1);
              }
              //Change node to immobile
              spatialPairs->changeMobility(*found, true);
            }
            else {
              //Change node to mobile
              spatialPairs->changeMobility(*found, false);
            }
          }
        }
      }
    });
  }
}