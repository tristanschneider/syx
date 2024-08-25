#include "Precompile.h"
#include "Constraints.h"

#include "AppBuilder.h"

#include "SpatialPairsStorage.h"
#include "generics/Enum.h"

namespace Constraints {
  template<class T>
  auto getOrAssert(const QueryAlias<T>& q, RuntimeDatabaseTaskBuilder& task, const TableID& table) {
    auto result = task.queryAlias(table, q);
    assert(result.size());
    return &result.get<0>(0);
  }

  ExternalTargetRowAlias identity(const ExternalTargetRowAlias& t) { return t; }
  ConstExternalTargetRowAlias toConst(const ExternalTargetRowAlias& t) { return t.read(); }

  template<class TargetT, auto transform>
  struct ResolveTargetT {
    TargetT operator()(NoTarget t) const {
      return t;
    }
    TargetT operator()(SelfTarget t) const {
      return t;
    }
    TargetT operator()(const ExternalTargetRowAlias& t) const {
      return getOrAssert(transform(t), task, table);
    }

    RuntimeDatabaseTaskBuilder& task;
    const TableID& table;
  };
  using ResolveTarget = ResolveTargetT<Rows::Target, &identity>;
  using ResolveConstTarget = ResolveTargetT<Rows::ConstTarget, &toConst>;

  Rows Definition::resolve(RuntimeDatabaseTaskBuilder& task, const TableID& table) {
    Rows result;
    ResolveTarget resolveTarget{ task, table };
    result.targetA = std::visit(resolveTarget, targetA);
    result.targetB = std::visit(resolveTarget, targetB);
    //A side is allowed to be empty but if it is specified the row must exist
    if(sideA) {
      result.sideA = getOrAssert(sideA, task, table);
    }
    if(sideB) {
      result.sideB = getOrAssert(sideB, task, table);
    }
    result.common = getOrAssert(common, task, table);
    return result;
  }

  Builder::Builder(const Rows& r)
    : rows{ r }
  {
  }

  Builder& Builder::select(const gnx::IndexRange& range) {
    return selected = range, *this;
  }

  class ConstraintStorageModifier : public IConstraintStorageModifier {
  public:
    ConstraintStorageModifier(RuntimeDatabaseTaskBuilder& task)
    {
      //Doesn't matter which table it reports to if there are multiple, so use the first
      if(auto tables = task.queryTables<ConstraintChangesRow>(); tables.size()) {
        changes = task.query<ConstraintChangesRow>(tables[0]).tryGetSingletonElement();
      }
    }

    void insert(const ElementRef& a, const ElementRef& b) final {
      const ConstraintPair pair{ a, b };
      if(changes && changes->trackedPairs.insert(pair).second) {
        changes->gained.push_back(pair);
      }
    }

    void erase(const ElementRef& a, const ElementRef& b) final {
      const ConstraintPair pair{ a, b };
      if(changes && changes->trackedPairs.erase(pair)) {
        changes->lost.push_back(pair);
      }
    }

    ConstraintChanges* changes{};
  };

  std::shared_ptr<IConstraintStorageModifier> createConstraintStorageModifier(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<ConstraintStorageModifier>(task);
  }

  void garbageCollect(IAppBuilder& builder) {
    auto task = builder.createTask();
    auto query = task.query<ConstraintChangesRow>();

    task.setCallback([query](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [changesRow] = query.get(t);
        ConstraintChanges& changes = changesRow->at();
        if(changes.ticksSinceGC++ > 200) {
          changes.ticksSinceGC = 0;
          for(const ConstraintPair& pair : changes.trackedPairs) {
            if(pair.a.isSet() && !pair.a || pair.b.isSet() && !pair.b) {
              changes.lost.push_back(pair);
            }
          }
          //Exit after any gc so there is never more than one per frame
          return;
        }
      }
    });

    builder.submitTask(std::move(task.setName("constraint gc")));
  }

  //TODO: duplication with Definition and Rows is annoying
  struct ConstraintTable {
    ConstraintTable(RuntimeDatabaseTaskBuilder& task, const Definition& definition, const TableID& table)
      : targetA{ std::visit(ResolveConstTarget{ task, table }, definition.targetA) }
      , targetB{ std::visit(ResolveConstTarget{ task, table }, definition.targetA) }
      , common{ getOrAssert(definition.common.read(), task, table) }
      , stableA{ &task.query<const StableIDRow>(table).get<0>(0) }
      , storage{ getOrAssert(definition.storage, task, table) }
    {
      if(definition.sideA) {
        sideA = getOrAssert(definition.sideA.read(), task, table);
      }
      if(definition.sideA) {
        sideB = getOrAssert(definition.sideB.read(), task, table);
      }
    }

    Rows::ConstTarget targetA, targetB;
    const ConstraintSideRow* sideA{};
    const ConstraintSideRow* sideB{};
    const ConstraintCommonRow* common{};
    const StableIDRow* stableA{};
    ConstraintStorageRow* storage{};
  };

  struct SpatialPairsTable {
    SP::ConstraintRow* manifold{};
    SP::PairTypeRow* pairType{};
  };

  void lazyInitStorage(
    size_t index,
    ConstraintStorage& storage,
    SpatialPairsTable& spatialPairs,
    const StableIDRow& stableA,
    const Rows::ConstTarget& targetA,
    const Rows::ConstTarget& targetB,
    const IslandGraph::Graph& graph) {
    ElementRef a, b;
    if(const auto* target = std::get_if<const ExternalTargetRow*>(&targetA)) {
      a = (*target)->at(index).target;
    }
    else if(std::holds_alternative<SelfTarget>(targetA)) {
      a = stableA.at(index);
    }
    if(const auto* target = std::get_if<const ExternalTargetRow*>(&targetB)) {
      b = (*target)->at(index).target;
    }
    auto edge = graph.findEdge(a, b);
    while(edge != graph.edgesEnd()) {
      //If this is an unclaimed constraint edge, claim it
      if(size_t i = *edge; i < spatialPairs.pairType->size()) {
        SP::PairType& type = spatialPairs.pairType->at(i);
        constexpr auto claimedBit = gnx::enumCast(SP::PairType::ClaimedBit);
        //This will match only if the claimed bit wasn't set already. It also excludes contact constraints
        if(type == SP::PairType::Constraint) {
          type = static_cast<SP::PairType>(gnx::enumCast(type) | claimedBit);
          storage.storageIndex = i;
          return;
        }
      }

      ++edge;
    }
  }

  //Copies the constraint information as configured by gameplay over to the SpatialPairsStorage
  void configureTable(const ConstraintTable& table, const IslandGraph::Graph& graph, SpatialPairsTable& spatialPairs) {
    if(std::holds_alternative<NoTarget>(table.targetA)) {
      assert(false && "side A must always point at something");
      return;
    }
    for(size_t i = 0; i < table.storage->size(); ++i) {
      ConstraintStorage& storage = table.storage->at(i);
      if(!storage.isValid()) {
        lazyInitStorage(i, storage, spatialPairs, *table.stableA, table.targetA, table.targetB, graph);
        continue;
      }
      SP::ConstraintManifold& manifold = spatialPairs.manifold->at(storage.storageIndex);

      //Solve if target is self or target is a valid external target
      bool shouldSolve = true;
      if(const auto* targets = std::get_if<const ExternalTargetRow*>(&table.targetA)) {
        shouldSolve = static_cast<bool>((*targets)->at(i).target);
      }

      if(shouldSolve) {
        manifold.sideA = table.sideA ? table.sideA->at(i) : ConstraintSide{};
        manifold.sideB = table.sideB ? table.sideB->at(i) : ConstraintSide{};
        manifold.common = table.common->at(i);
      }
      else {
        manifold.common.lambdaMin = manifold.common.lambdaMax = 0.0f;
      }
    }
  }

  //TODO: is this complexity worth it compared to having the existing narrowphase switch off of the pairtype?
  void constraintNarrowphase(IAppBuilder& builder) {
    auto task = builder.createTask();
    auto definitions = task.query<const TableConstraintDefinitionsRow>();
    std::vector<ConstraintTable> constraintTables;
    constraintTables.reserve(definitions.size());
    for(size_t t = 0; t < constraintTables.size(); ++t) {
      auto [def] = definitions.get(t);
      for(const Definition& definition : def->at().definitions) {
        constraintTables.push_back(ConstraintTable{ task, definition, definitions.matchingTableIDs[t] });
      }
    }
    auto sp = task.query<const SP::IslandGraphRow, SP::PairTypeRow, SP::ConstraintRow>();
    assert(sp.size());
    auto [g, p, c] = sp.get(0);
    SpatialPairsTable spatialPairs{ c, p };
    const IslandGraph::Graph* graph = &g->at();

    task.setCallback([graph, constraintTables, spatialPairs](AppTaskArgs&) mutable {
      //TODO: multithreaded task
      for(const ConstraintTable& table : constraintTables) {
        configureTable(table, *graph, spatialPairs);
      }
    });

    builder.submitTask(std::move(task.setName("constraint narrowphase")));
  }
}