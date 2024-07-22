#include "Precompile.h"
#include "SpatialPairsStorage.h"

#include "AppBuilder.h"
#include "Narrowphase.h"
#include "SweepNPruneBroadphase.h"

namespace SP {
  float ZInfo::getOverlap() const {
    return separation <= Narrowphase::Z_OVERLAP_TOLERANCE + 0.00001f;
  }

  bool ZContactManifold::isTouching() const {
    return info && info->getOverlap() > 0.0f;
  }

  size_t addIslandEdge(ITableModifier& modifier,
    IslandGraph::Graph& graph,
    ObjA& rowA,
    ObjB& rowB,
    const ElementRef& a,
    const ElementRef& b
  ) {
    //Add the edge pointing at the spatial pair to the island graph
    const IslandGraph::EdgeUserdata entryIndex = IslandGraph::addUnmappedEdge(graph, a, b);

    if(rowA.size() <= entryIndex) {
      //Make plenty of space. No harm in a little extra
      modifier.resize(entryIndex + 100);
    }
    //Assign new mappings to the destination spatial pair
    rowA.at(entryIndex) = a;
    rowB.at(entryIndex) = b;
    return entryIndex;
  }

  void updateSpatialPairsFromBroadphase(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("update spatial pairs");
    const auto dstTable = builder.queryTables<ObjA, ObjB, ManifoldRow>().matchingTableIDs[0];
    auto dstQuery = task.query<ObjA, ObjB, IslandGraphRow>(dstTable);
    auto dstModifier = task.getModifierForTable(dstTable);
    auto ids = task.getIDResolver();
    auto broadphaseChanges = task.query<SharedRow<SweepNPruneBroadphase::PairChanges>>();

    task.setCallback([dstQuery, dstModifier, ids, broadphaseChanges](AppTaskArgs&) mutable {
      auto [objA, objB, islandGraph] = dstQuery.get(0);
      IslandGraph::Graph& graph = islandGraph->at();
      for(size_t t = 0; t < broadphaseChanges.size(); ++t) {
        SweepNPruneBroadphase::PairChanges& changes = broadphaseChanges.get<0>(t).at();

        //Add new edges and spatial pairs for all new pairs
        for(size_t i = 0; i < changes.mGained.size(); ++i) {
          const auto& gain = changes.mGained[i];
          const ElementRef a = gain.a;
          const ElementRef b = gain.b;
          auto it = graph.findEdge(a, b);
          //This is a hack that shouldn't happen but sometimes the broadphase seems to report duplicates
          if(it == graph.edgesEnd()) {
            addIslandEdge(*dstModifier, graph, *objA, *objB, a, b);
          }
          else {
            assert(false);
          }
        }

        //Remove all edges corresponding to the lost pairs
        for(const auto& loss : changes.mLost) {
          const ElementRef a = loss.a;
          const ElementRef b = loss.b;
          auto edge = graph.findEdge(a, b);
          if(edge != graph.edgesEnd()) {
            //Mark the spatial pair as removed
            if(objA->size() > *edge) {
              objA->at(*edge) = objB->at(*edge) = {};
            }

            //Remove the graph edge
            IslandGraph::removeEdge(graph, edge);
          }
          else {
            //This can happen if one of the nodes was destroyed because the broadphase reports the loss
            //after the island graph removes the node. If it happened while both exist something went wrong with pair tracking
            assert(!a || !b);
          }
        }
      }
    });
    builder.submitTask(std::move(task));
  }

  struct StorageModifier : IStorageModifier {
    StorageModifier(RuntimeDatabaseTaskBuilder& task)
      : graph{ *task.query<IslandGraphRow>().tryGetSingletonElement() }
      , resolver{ task.getIDResolver() }
    {}

    constexpr static IslandGraph::IslandPropagationMask getMask(bool isImmobile) {
      return isImmobile ? IslandGraph::PROPAGATE_NONE : IslandGraph::PROPAGATE_ALL;
    }

    void addSpatialNode(const ElementRef& node, bool isImmobile) override {
      assert(node && "Ref should exist if adding it");
      IslandGraph::addNode(graph, node, getMask(isImmobile));
    }

    void removeSpatialNode(const ElementRef& node) override {
      IslandGraph::removeNode(graph, node);
    }

    void changeMobility(const ElementRef& node, bool isImmobile) override {
      assert(node);
      auto it = graph.findNode(node);
      IslandGraph::setPropagation(graph, it, getMask(isImmobile));
    }

    IslandGraph::Graph& graph;
    std::shared_ptr<IIDResolver> resolver;
  };
  constexpr size_t TEST = sizeof(ElementRef);
  std::shared_ptr<IStorageModifier> createStorageModifier(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<StorageModifier>(task);
  }
}