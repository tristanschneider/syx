#include "Precompile.h"
#include "SpatialPairsStorage.h"

#include "AppBuilder.h"
#include "Narrowphase.h"
#include "SweepNPruneBroadphase.h"

namespace SP {
  float ZInfo::getOverlap() const {
    return -separation;
  }

  bool ZContactManifold::isTouching() const {
    return info.separation <= Narrowphase::Z_OVERLAP_TOLERANCE + 0.00001f;
  }

  void ZContactManifold::clear() {
    info.normal = info.separation = 0;
  }

  bool ZContactManifold::isSet() const {
    //Match the values of clear
    return info.normal != 0 || info.separation != 0;
  }

  size_t addIslandEdge(ITableModifier& modifier,
    IslandGraph::Graph& graph,
    ObjA& rowA,
    ObjB& rowB,
    PairTypeRow& rowType,
    const ElementRef& a,
    const ElementRef& b,
    PairType type
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
    rowType.at(entryIndex) = type;
    return entryIndex;
  }

  template<class T, class GainT, class LossT>
  concept SpatialAdapter = requires(T adapter, GainT gain, LossT loss, const IslandGraph::Graph& graph) {
    { adapter.unwrapGain(gain) } -> std::convertible_to<std::pair<ElementRef, ElementRef>>;
    { adapter.unwrapLoss(graph, loss) } -> std::convertible_to<IslandGraph::Graph::ConstEdgeIterator>;
  };

  struct ContactAdapter {
    std::pair<ElementRef, ElementRef> unwrapGain(const Broadphase::SweepCollisionPair& pair) const {
      return std::make_pair(pair.a, pair.b);
    }

    IslandGraph::Graph::ConstEdgeIterator unwrapLoss(const IslandGraph::Graph& graph, const Broadphase::SweepCollisionPair& pair) const {
      auto edge = graph.findEdge(pair.a, pair.b);
      while(edge != graph.edgesEnd() && !isContactPair(pairTypes.at(*edge))) {
        ++edge;
      }
      return edge.toEdgeIterator();
    }

    const PairTypeRow& pairTypes;
  };

  struct ConstraintAdapter {
    std::pair<ElementRef, ElementRef> unwrapGain(const Constraints::ConstraintPair& pair) const {
      return std::make_pair(pair.a, pair.b);
    }

    IslandGraph::Graph::ConstEdgeIterator unwrapLoss(const IslandGraph::Graph& graph, const Constraints::ConstraintStorage& storage) const {
      return graph.findEdge(storage.getHandle());
    }
  };

  template<class GainT, class LossT, SpatialAdapter<GainT, LossT> AdapterT>
  void trackEdges(
    const std::vector<GainT>& gained,
    const std::vector<LossT>& lost,
    IslandGraph::Graph& graph,
    ITableModifier& pairStorageModifier,
    PairType type,
    ObjA& rowA,
    ObjB& rowB,
    PairTypeRow& rowType,
    const AdapterT& adapter
  ) {
    //Add new edges and spatial pairs for all new pairs
    //Multiple edges may exist for the same pair for constraint type edges
    //It is expected that there is only a single edge for contact types
    for(const GainT& gain : gained) {
      auto [a, b] = adapter.unwrapGain(gain);
      addIslandEdge(pairStorageModifier, graph, rowA, rowB, rowType, a, b, type);
    }

    //Remove all edges corresponding to the lost pairs
    for(const auto& loss : lost) {
      const IslandGraph::Graph::ConstEdgeIterator edge = adapter.unwrapLoss(graph, loss);
      if(edge != graph.cEdgesEnd()) {
        //Mark the spatial pair as removed
        if(rowA.size() > *edge) {
          rowA.at(*edge) = rowB.at(*edge) = {};
        }

        //Remove the graph edge
        IslandGraph::removeEdge(graph, edge);
      }
    }
  }

  void updateSpatialPairsFromBroadphase(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("update spatial pairs");
    const auto dstTable = builder.queryTables<ObjA, ObjB, PairTypeRow, ManifoldRow>()[0];
    auto dstQuery = task.query<ObjA, ObjB, PairTypeRow, IslandGraphRow>(dstTable);
    auto dstModifier = task.getModifierForTable(dstTable);
    auto ids = task.getIDResolver();
    auto broadphaseChanges = task.query<SharedRow<SweepNPruneBroadphase::PairChanges>>();
    auto constraintChanges = task.query<Constraints::ConstraintChangesRow>();

    task.setCallback([dstQuery, dstModifier, ids, broadphaseChanges, constraintChanges](AppTaskArgs&) mutable {
      auto [objA, objB, pairType, islandGraph] = dstQuery.get(0);
      IslandGraph::Graph& graph = islandGraph->at();
      for(size_t t = 0; t < broadphaseChanges.size(); ++t) {
        SweepNPruneBroadphase::PairChanges& changes = broadphaseChanges.get<0>(t).at();
        trackEdges(changes.mGained, changes.mLost, graph, *dstModifier, PairType::ContactXY, *objA, *objB, *pairType, ContactAdapter{ *pairType });
      }
      for(size_t t = 0; t < constraintChanges.size(); ++t) {
        Constraints::ConstraintChanges& changes = constraintChanges.get<0>(t).at();
        trackEdges(changes.gained, changes.lost, graph, *dstModifier, PairType::Constraint, *objA, *objB, *pairType, ConstraintAdapter{});
        changes.gained.clear();
        changes.lost.clear();
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

    size_t nodeCount() const final {
      //Expect a single "invalid" node to always exist
      return graph.nodes.activeSize() - 1;
    }

    size_t edgeCount() const final {
      return graph.edges.activeSize();
    }

    IslandGraph::Graph& graph;
    std::shared_ptr<IIDResolver> resolver;
  };

  std::shared_ptr<IStorageModifier> createStorageModifier(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<StorageModifier>(task);
  }
}