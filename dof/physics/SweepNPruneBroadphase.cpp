#include "Precompile.h"
#include "SweepNPruneBroadphase.h"

#include "Profile.h"

namespace SweepNPruneBroadphase {
  std::optional<std::pair<StableElementID, StableElementID>> _tryGetOrderedCollisionPair(const Broadphase::SweepCollisionPair& key, const PhysicsTableIds& tableIds, StableElementMappings& stableMappings, bool assertIfMissing) {
    auto elementA = stableMappings.findKey(key.a.value);
    auto elementB = stableMappings.findKey(key.b.value);
    if(assertIfMissing) {
      assert(elementA);
      assert(elementB);
    }
    if(elementA && elementB) {
      const StableElementID originalA{ elementA->second, elementA->first };
      const StableElementID originalB{ elementB->second, elementB->first };
      auto pair = std::make_pair(originalA, originalB);
      //If this isn't an applicable pair, skip to the next without incrementing addIndex
      if(CollisionPairOrder::tryOrderCollisionPair(pair.first, pair.second, tableIds)) {
        return pair;
      }
    }
    return {};
  }

  TaskRange updateBoundaries(Broadphase::SweepGrid::Grid& grid, std::vector<BoundariesQuery> query, const BoundariesConfig& cfg) {
    struct Temp {
      struct Query {
        std::vector<float> minX, maxX, minY, maxY;
      };
      std::vector<Query> data;
      std::vector<BoundariesQuery> query;
    };
    auto t = std::make_shared<Temp>();
    t->query = std::move(query);
    t->data.resize(t->query.size());

    auto root = TaskNode::createEmpty();
    auto computeX = TaskNode::create([cfg, t](...) mutable {
      PROFILE_SCOPE("physics", "recomputeBoundaryX");
      const float halfSize = cfg.mHalfSize + cfg.mPadding;
      for(size_t i = 0; i < t->query.size(); ++i) {
        std::vector<float>& min = t->data[i].minX;
        std::vector<float>& max = t->data[i].maxX;
        BoundariesQuery& q = t->query[i];

        min.resize(q.posX->size());
        max.resize(q.posX->size());
        for(size_t c = 0; c < q.posX->size(); ++c) {
          const float p = q.posX->at(c);
          min[c] = p - halfSize;
          max[c] = p + halfSize;
        }
      }
    });
    auto computeY = TaskNode::create([cfg, t](...) mutable {
      PROFILE_SCOPE("physics", "recomputeBoundaryY");
      const float halfSize = cfg.mHalfSize + cfg.mPadding;
      for(size_t i = 0; i < t->query.size(); ++i) {
        std::vector<float>& min = t->data[i].minY;
        std::vector<float>& max = t->data[i].maxY;
        BoundariesQuery& q = t->query[i];

        min.resize(q.posY->size());
        max.resize(q.posY->size());
        for(size_t c = 0; c < q.posY->size(); ++c) {
          const float p = q.posY->at(c);
          min[c] = p - halfSize;
          max[c] = p + halfSize;
        }
      }
    });

    auto update = TaskNode::create([&grid, t](...) mutable {
      PROFILE_SCOPE("physics", "assignBoundaries");
      for(size_t i = 0; i < t->query.size(); ++i) {
        Broadphase::SweepGrid::updateBoundaries(grid,
          t->data[i].minX.data(),
          t->data[i].maxX.data(),
          t->data[i].minY.data(),
          t->data[i].maxY.data(),
          t->query[i].keys->mElements.data(),
          t->data[i].minX.size()
        );
      }
    });
    root->mChildren.push_back(computeX);
    root->mChildren.push_back(computeY);
    TaskBuilder::_addSyncDependency(*root, update);
    return TaskBuilder::addEndSync(root);
  }

  TaskRange computeCollisionPairs(BroadphaseTable& broadphase) {
    auto& grid = std::get<SharedRow<Broadphase::SweepGrid::Grid>>(broadphase.mRows).at();
    auto& pairChanges = std::get<SharedRow<SweepNPruneBroadphase::PairChanges>>(broadphase.mRows).at();
    return Broadphase::SweepGrid::recomputePairs(grid, { pairChanges.mGained, pairChanges.mLost });
  }
}