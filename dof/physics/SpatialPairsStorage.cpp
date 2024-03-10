#include "Precompile.h"
#include "SpatialPairsStorage.h"

#include "AppBuilder.h"
#include "SweepNPruneBroadphase.h"

namespace SP {
  size_t addIslandEdge(ITableModifier& modifier,
    IslandGraph::Graph& graph,
    ObjA& rowA,
    ObjB& rowB,
    const StableElementID& a,
    const StableElementID& b
  ) {
    //Add the edge pointing at the spatial pair to the island graph
    const IslandGraph::EdgeUserdata entryIndex = IslandGraph::addUnmappedEdge(graph, a.mStableID, b.mStableID);

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
          const StableElementID a{ StableElementID::fromStableID(gain.a) };
          const StableElementID b{ StableElementID::fromStableID(gain.b) };
          auto it = graph.findEdge(a.mStableID, b.mStableID);
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
          const StableElementID a{ StableElementID::fromStableID(loss.a) };
          const StableElementID b{ StableElementID::fromStableID(loss.b) };
          auto edge = graph.findEdge(a.mStableID, b.mStableID);
          if(edge != graph.edgesEnd()) {
            //Mark the spatial pair as removed
            if(objA->size() > *edge) {
              objA->at(*edge) = objB->at(*edge) = StableElementID::invalid();
            }

            //Remove the graph edge
            IslandGraph::removeEdge(graph, edge);
          }
          else {
            //This can happen if one of the nodes was destroyed because the broadphase reports the loss
            //after the island graph removes the node. If it happened while both exist something went wrong with pair tracking
            assert(!ids->tryResolveStableID(a) || !ids->tryResolveStableID(b));
          }
        }
      }
    });
    builder.submitTask(std::move(task));
  }

  struct StorageModifier : IStorageModifier {
    StorageModifier(RuntimeDatabaseTaskBuilder& task)
      : graph{ *task.query<IslandGraphRow>().tryGetSingletonElement() }
    {}

    constexpr static IslandGraph::IslandPropagationMask getMask(bool isImmobile) {
      return isImmobile ? IslandGraph::PROPAGATE_NONE : IslandGraph::PROPAGATE_ALL;
    }

    void addSpatialNode(const StableElementID& node, bool isImmobile) override {
      IslandGraph::addNode(graph, node.mStableID, getMask(isImmobile));
    }

    void removeSpatialNode(const StableElementID& node) override {
      IslandGraph::removeNode(graph, node.mStableID);
    }

    void changeMobility(const StableElementID& node, bool isImmobile) override {
      auto it = graph.findNode(node.mStableID);
      graph.nodes[it.node].propagation = getMask(isImmobile);
    }

    IslandGraph::Graph& graph;
  };

  std::shared_ptr<IStorageModifier> createStorageModifier(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<StorageModifier>(task);
  }
}