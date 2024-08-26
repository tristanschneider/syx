#include "Precompile.h"
#include "Constraints.h"

#include "AppBuilder.h"

#include "SpatialPairsStorage.h"
#include "generics/Enum.h"
#include "generics/Container.h"

namespace Constraints {
  const TableConstraintDefinitions* getOrAssertDefinitions(RuntimeDatabaseTaskBuilder& task, const TableID& table) {
    const TableConstraintDefinitions* result = task.query<const TableConstraintDefinitionsRow>(table).tryGetSingletonElement();
    assert(result);
    return result;
  }

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
    ConstraintStorageModifier(
      RuntimeDatabaseTaskBuilder& task,
      ConstraintDefinitionKey constraintKey,
      const TableID& constraintTable
    )
      : changes{ task.query<ConstraintChangesRow>(constraintTable).tryGetSingletonElement() }
      , key{ constraintKey }
    {
      const TableConstraintDefinitions* definitions = getOrAssertDefinitions(task, constraintTable);
      assert(changes && key < changes->pendingConstraints.size() && key < definitions->definitions.size());
      storage = &task.queryAlias(constraintTable, definitions->definitions[key].storage).get<0>(0);
      stable = &task.query<const StableIDRow>(constraintTable).get<0>(0);
    }

    void insert(size_t tableIndex, const ElementRef& a, const ElementRef& b) final {
      const ConstraintPair pair{ a, b };
      //Immediately clear any storage that may have been here. Ownership will be managed by GC
      //If it was already pending, the first will be created, then the second, then the first cleared by GC
      storage->at(tableIndex).setPending();
      //Request creation of SpatialPairsStorage
      //TODO: should this be here or part of assignStorage?
      changes->gained.push_back(pair);
      //Track this internally so it can eventually be released via GC
      changes->pendingConstraints[key].constraints.push_back(PendingConstraint{ stable->at(tableIndex), a, b });
    }

    void erase(size_t tableIndex, const ElementRef& a, const ElementRef& b) final {
      const ConstraintPair pair{ a, b };
      //Clear the storage. GC will see this as a tracked constraint with no storage and remove it
      //If it was pending, assignment will see it was cleared rather than pending and immediately delete the created storage
      storage->at(tableIndex).clear();
    }

    ConstraintChanges* changes{};
    ConstraintStorageRow* storage{};
    ConstraintDefinitionKey key{};
    const StableIDRow* stable{};
  };

  std::shared_ptr<IConstraintStorageModifier> createConstraintStorageModifier(RuntimeDatabaseTaskBuilder& task, ConstraintDefinitionKey constraintKey, const TableID& constraintTable) {
    return std::make_shared<ConstraintStorageModifier>(task, constraintKey, constraintTable);
  }

  struct ConstraintOwnershipTable {
    ConstraintOwnershipTable(RuntimeDatabaseTaskBuilder& task, const ConstraintDefinitionKey k, const Definition& definition, const TableID& table)
      : key{ k }
      , changes{ task.query<ConstraintChangesRow>(table).tryGetSingletonElement() }
      , storage{ getOrAssert(definition.storage, task, table) }
    {
      assert(changes);
    }

    ConstraintDefinitionKey key{};
    ConstraintChanges* changes{};
    ConstraintStorageRow* storage{};
  };

  template<class T>
  std::vector<T> queryConstraintTables(RuntimeDatabaseTaskBuilder& task) {
    auto definitions = task.query<const TableConstraintDefinitionsRow>();
    std::vector<T> constraintTables;
    constraintTables.reserve(definitions.size());
    for(size_t t = 0; t < constraintTables.size(); ++t) {
      auto [def] = definitions.get(t);
      for(size_t i = 0; i < def->at().definitions.size(); ++i) {
        constraintTables.push_back(T{ task, i, def->at().definitions[i], definitions.matchingTableIDs[t] });
      }
    }
    return constraintTables;
  }

  void initDefinition(IAppBuilder& builder) {
    auto temp = builder.createTask();
    temp.discard();
    auto defs = temp.query<TableConstraintDefinitionsRow, ConstraintChangesRow>();
    for(size_t t = 0; t < defs.size(); ++t) {
      auto [def, changes] = defs.get(t);
      ConstraintChanges& c = changes->at();
      TableConstraintDefinitions& d = def->at();
      c.pendingConstraints.resize(d.definitions.size());
      c.trackedConstraints.resize(d.definitions.size());
    }
  }

  void assignStorage(IAppBuilder& builder) {
    initDefinition(builder);

    auto task = builder.createTask();
    std::vector<ConstraintOwnershipTable> constraints = queryConstraintTables<ConstraintOwnershipTable>(task);
    auto sp = task.query<const SP::IslandGraphRow, const SP::PairTypeRow, SP::ConstraintRow>();
    ElementRefResolver res = task.getIDResolver()->getRefResolver();

    task.setCallback([constraints, sp, res](AppTaskArgs&) mutable {
      auto [g, pairTypes, c] = sp.get(0);
      const IslandGraph::Graph& graph = g->at();
      for(const ConstraintOwnershipTable& table : constraints) {
        PendingDefinitionConstraints& pending = table.changes->pendingConstraints[table.key];
        OwnedDefinitionConstraints& trackedConstraints = table.changes->trackedConstraints[table.key];
        for(size_t i = 0; i < pending.constraints.size();) {
          const PendingConstraint& p = pending.constraints[i];
          auto it = graph.findEdge(p.a, p.b);
          while(it != graph.edgesEnd()) {
            const size_t spatialPairsIndex = *it;
            //Shouldn't generally happen
            if(spatialPairsIndex >= pairTypes->size()) {
              continue;
            }
            //Skip unrelated edge types
            if(pairTypes->at(spatialPairsIndex) != SP::PairType::Constraint) {
              continue;
            }
            //Skip edges that are already taken, take the first available
            if(table.changes->ownedEdges.insert(spatialPairsIndex).second) {
              break;
            }
          }

          //If storage was found, move it from pending to tracked
          if(it != graph.edgesEnd()) {
            //Assign storage so it can be used for configureConstraints.
            if(auto self = res.tryUnpack(p.owner)) {
              ConstraintStorage& ownerStorage = table.storage->at(self->getElementIndex());
              if(ownerStorage.isPending()) {
                ownerStorage.assign(*it);
              }
            }
            //For simplicity, this is always moved to trackedConstraints, even if storage above wasn't found
            //This could happen if a constraint is immediately deleted
            //Either way, it means the code path for deletion can always go through GC rather than a special step here
            trackedConstraints.constraints.push_back(OwnedConstraint{ p.owner, ConstraintStorage{ *it } });

            gnx::Container::swapRemove(pending.constraints, i);
          }
          //If nothing was found, try again later
          else {
            ++i;
          }
        }
      }
    });

    builder.submitTask(std::move(task.setName("constraint assign")));
  }

  std::pair<ElementRef, ElementRef> getEdge(const IslandGraph::Graph& graph, size_t edgeIndex) {
    if(graph.edges.size() > edgeIndex) {
      const auto& e = graph.edges[edgeIndex];
      assert(e.data == edgeIndex);
      assert(e.nodeA < graph.nodes.size() && e.nodeB < graph.nodes.size());
      return { graph.nodes[e.nodeA].data, graph.nodes[e.nodeB].data };
    }
    return {};
  }

  void garbageCollect(IAppBuilder& builder) {
    auto task = builder.createTask();
    std::vector<ConstraintOwnershipTable> constraints = queryConstraintTables<ConstraintOwnershipTable>(task);
    ElementRefResolver res = task.getIDResolver()->getRefResolver();
    const IslandGraph::Graph* graph = task.query<const SP::IslandGraphRow>().tryGetSingletonElement();
    assert(graph);

    task.setCallback([constraints, res, graph](AppTaskArgs&) {
      for(const ConstraintOwnershipTable& table : constraints) {
        OwnedDefinitionConstraints& trackedConstraints = table.changes->trackedConstraints[table.key];
        if(trackedConstraints.ticksSinceGC++ < 200) {
          continue;
        }

        for(size_t i = 0; i < trackedConstraints.constraints.size();) {
          const OwnedConstraint& constraint = trackedConstraints.constraints[i];
          //Ensure the owner still exists
          if(auto unpacked = res.tryUnpack(constraint.owner)) {
            const ConstraintStorage& storage = table.storage->at(unpacked->getElementIndex());
            //Ensure the owner is still pointing at this constraint, could either be cleared or a newer one
            if(constraint.storage == storage) {
              //Ensure the members of the constraint exist
              auto [a, b] = getEdge(*graph, constraint.storage.storageIndex);
              //A must exist, B is allowed to be unset but not invalid
              if(a && b.isUnsetOrValid()) {
                ++i;
                continue;
              }
            }
          }

          //If we made it here the entry is invalid, remove it
          table.changes->ownedEdges.erase(constraint.storage.storageIndex);
          table.changes->lost.push_back(constraint.storage);
          gnx::Container::swapRemove(trackedConstraints.constraints, i);
        }

        //Exit after single GC so they don't all land on the same frame
        break;
      }
    });

    builder.submitTask(std::move(task.setName("constraint gc")));
  }

  //TODO: duplication with Definition and Rows is annoying
  struct ConstraintTable {
    ConstraintTable(RuntimeDatabaseTaskBuilder& task, ConstraintDefinitionKey, const Definition& definition, const TableID& table)
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
    const ConstraintStorageRow* storage{};
  };

  struct SpatialPairsTable {
    SP::ConstraintRow* manifold{};
    SP::PairTypeRow* pairType{};
  };

  //Copies the constraint information as configured by gameplay over to the SpatialPairsStorage
  void configureTable(const ConstraintTable& table, SpatialPairsTable& spatialPairs) {
    if(std::holds_alternative<NoTarget>(table.targetA)) {
      assert(false && "side A must always point at something");
      return;
    }
    for(size_t i = 0; i < table.storage->size(); ++i) {
      const ConstraintStorage& storage = table.storage->at(i);
      if(!storage.isValid()) {
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
    std::vector<ConstraintTable> constraintTables = queryConstraintTables<ConstraintTable>(task);
    auto sp = task.query<const SP::IslandGraphRow, SP::PairTypeRow, SP::ConstraintRow>();
    assert(sp.size());
    auto [g, p, c] = sp.get(0);
    SpatialPairsTable spatialPairs{ c, p };

    task.setCallback([constraintTables, spatialPairs](AppTaskArgs&) mutable {
      //TODO: multithreaded task
      for(const ConstraintTable& table : constraintTables) {
        configureTable(table, spatialPairs);
      }
    });

    builder.submitTask(std::move(task.setName("constraint narrowphase")));
  }
}