#include "Precompile.h"
#include "SpatialPairsStorage.h"

#include "AppBuilder.h"
#include "SweepNPruneBroadphase.h"

namespace SP {
  void updateSpatialPairsFromBroadphase(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("update spatial pairs");
    const auto dstTable = builder.queryTables<ObjA, ObjB, ManifoldRow>().matchingTableIDs[0];
    auto dstQuery = task.query<ObjA, ObjB, IslandGraphRow, const StableIDRow>(dstTable);
    auto dstModifier = task.getModifierForTable(dstTable);
    auto ids = task.getIDResolver();
    auto broadphaseChanges = task.query<SharedRow<SweepNPruneBroadphase::PairChanges>>();

    task.setCallback([dstQuery, dstModifier, ids, broadphaseChanges](AppTaskArgs&) mutable {
      auto [objA, objB, islandGraph, stableIds] = dstQuery.get(0);
      IslandGraph::Graph& graph = islandGraph->at();
      for(size_t t = 0; t < broadphaseChanges.size(); ++t) {
        SweepNPruneBroadphase::PairChanges& changes = broadphaseChanges.get<0>(t).at();

        //Create space for all the new entries
        const size_t dstBegin = dstModifier->addElements(changes.mGained.size());
        std::vector<StableElementID> undo;
        //Add new edges and spatial pairs for all new pairs
        for(size_t i = 0; i < changes.mGained.size(); ++i) {
          const auto& gain = changes.mGained[i];
          const StableElementID a{ StableElementID::fromStableID(gain.a) };
          const StableElementID b{ StableElementID::fromStableID(gain.b) };
          auto it = graph.findEdge(a.mStableID, b.mStableID);
          //This is a hack that shouldn't happen but sometimes the broadphase seems to report duplicates
          if(it == graph.edgesEnd()) {
            //Assign new mappings to the destination spatial pair
            const size_t pi = dstBegin + i;
            const StableElementID spatialPair{ StableElementID::fromStableRow(pi, *stableIds) };
            objA->at(pi) = a;
            objB->at(pi) = b;

            //Add the edge pointing at the spatial pair to the island graph
            IslandGraph::addEdge(graph, a.mStableID, b.mStableID, spatialPair.mStableID);
          }
          else {
            //TODO: still happens when there is a gain and loss on the same frame, but how is that possible?
            assert(false);
            undo.push_back(StableElementID::fromStableRow(dstBegin + i, *stableIds));
          }
        }

        //TODO: find a way to get rid of the need for this
        for(const auto& id : undo) {
          dstModifier->swapRemove(ids->tryResolveAndUnpack(id)->unpacked);
        }

        //Remove all edges corresponding to the lost pairs
        for(const auto& loss : changes.mLost) {
          const StableElementID a{ StableElementID::fromStableID(loss.a) };
          const StableElementID b{ StableElementID::fromStableID(loss.b) };
          auto edge = graph.findEdge(a.mStableID, b.mStableID);
          if(edge != graph.edgesEnd()) {
            //Remove the spatial pair
            if(auto resolved = ids->tryResolveAndUnpack(StableElementID::fromStableID(*edge))) {
              dstModifier->swapRemove(resolved->unpacked);
            }
            else {
              assert(false);
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