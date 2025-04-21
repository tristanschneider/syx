#include "Precompile.h"
#include "Constraints.h"

#include "AppBuilder.h"

#include "Events.h"
#include "SpatialPairsStorage.h"
#include "generics/Enum.h"
#include "generics/Container.h"
#include "Physics.h"
#include "TransformResolver.h"
#include "Geometric.h"
#include "ConstraintSolver.h"
#include "TLSTaskImpl.h"

namespace Constraints {
  constexpr size_t GC_TRACK_LIMIT = 200;

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

  Rows Definition::resolve(RuntimeDatabaseTaskBuilder& task, const TableID& table) const {
    Rows result;
    ResolveTarget resolveTarget{ task, table };
    result.targetA = std::visit(resolveTarget, targetA);
    result.targetB = std::visit(resolveTarget, targetB);
    if(custom) {
      result.custom = getOrAssert(custom, task, table);
    }
    result.joint = getOrAssert(joint, task, table);
    return result;
  }

  Rows Definition::resolve(RuntimeDatabaseTaskBuilder& task, const TableID& table, ConstraintDefinitionKey key) {
    if(auto q = task.query<const TableConstraintDefinitionsRow>(table); q.size()) {
      const TableConstraintDefinitions& defs = q.get<0>(0).at();
      if(key < defs.definitions.size()) {
        return defs.definitions[key].resolve(task, table);
      }
    }
    return {};
  }

  Builder::Builder(const Rows& r)
    : rows{ r }
  {
  }

  Builder& Builder::select(const gnx::IndexRange& range) {
    return selected = range, *this;
  }

  Builder& Builder::setJointType(const JointVariant& joint) {
    for(auto i : selected) {
      rows.joint->at(i) = joint;
    }
    return *this;
  }

  Builder& Builder::setTargets(const ElementRef& a, const ElementRef& b) {
    auto* targetA = std::get_if<ExternalTargetRow*>(&rows.targetA);
    auto* targetB = std::get_if<ExternalTargetRow*>(&rows.targetB);
    //This only makes sense to call if there is a place to store the specified target
    assert((targetA && *targetA) || !a);
    assert((targetB && *targetB) || !b);
    for(auto i : selected) {
      if(a && targetA) {
        (*targetA)->at(i) = Constraints::ExternalTarget{ a };
      }
      if(b && targetB) {
        (*targetB)->at(i) = Constraints::ExternalTarget{ b };
      }
    }
    return *this;
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
      //One-sided constraints are okay but if so they can only be A. If A is emptpy the caller messed up
      assert(a);
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
      changes->trackedConstraints[key].gcRate.forceUpdate();
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
      : tableID{ table }
      , key{ k }
      , changes{ task.query<ConstraintChangesRow>(table).tryGetSingletonElement() }
      , storage{ getOrAssert(definition.storage, task, table) }
    {
      assert(changes);
    }

    TableID tableID{};
    ConstraintDefinitionKey key{};
    ConstraintChanges* changes{};
    ConstraintStorageRow* storage{};
  };

  template<class T>
  std::vector<T> queryConstraintTables(RuntimeDatabaseTaskBuilder& task) {
    auto definitions = task.query<const TableConstraintDefinitionsRow>();
    std::vector<T> constraintTables;
    constraintTables.reserve(definitions.size());
    for(size_t t = 0; t < definitions.size(); ++t) {
      auto [def] = definitions.get(t);
      for(size_t i = 0; i < def->at().definitions.size(); ++i) {
        constraintTables.push_back(T{ task, i, def->at().definitions[i], definitions[t] });
      }
    }
    return constraintTables;
  }

  void init(IAppBuilder& builder) {
    auto task = builder.createTask();
    auto defs = task.query<TableConstraintDefinitionsRow, ConstraintChangesRow>();

    task.setCallback([defs](AppTaskArgs&) mutable {
      for(size_t t = 0; t < defs.size(); ++t) {
        auto [def, changes] = defs.get(t);
        ConstraintChanges& c = changes->at();
        TableConstraintDefinitions& d = def->at();
        c.pendingConstraints.resize(d.definitions.size());
        c.trackedConstraints.resize(d.definitions.size());
      }
    });

    builder.submitTask(std::move(task.setName("init constraints")));
  }

  void assignStorage(IAppBuilder& builder) {
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
          for(; it != graph.edgesEnd(); ++it) {
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
            const auto edgeHandle = graph.edges.getHandle(*it);
            //Assign storage so it can be used for configureConstraints.
            if(auto self = res.tryUnpack(p.owner)) {
              ConstraintStorage& ownerStorage = table.storage->at(self->getElementIndex());
              if(ownerStorage.isPending()) {
                ownerStorage.assign(edgeHandle);
              }
            }
            //For simplicity, this is always moved to trackedConstraints, even if storage above wasn't found
            //This could happen if a constraint is immediately deleted
            //Either way, it means the code path for deletion can always go through GC rather than a special step here
            trackedConstraints.constraints.push_back(OwnedConstraint{ p.owner, ConstraintStorage{ edgeHandle } });

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

  std::pair<ElementRef, ElementRef> getEdge(const IslandGraph::Graph& graph, const std::pair<IslandGraph::EdgeIndex, IslandGraph::EdgeVersion>& edgeIndex) {
    if(const IslandGraph::Edge* e = graph.edges.tryGet(edgeIndex)) {
      assert(e->data == edgeIndex.first);
      assert(e->nodeA < graph.nodes.addressableSize() && e->nodeB < graph.nodes.addressableSize());
      return { graph.nodes[e->nodeA].data, graph.nodes[e->nodeB].data };
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
        if(!trackedConstraints.gcRate.tryUpdate()) {
          continue;
        }

        for(size_t i = 0; i < trackedConstraints.constraints.size();) {
          const OwnedConstraint& constraint = trackedConstraints.constraints[i];
          //Ensure the owner still exists and is in this table
          //TODO: potential loss of constraint if object moves between two different constraint tables
          if(auto unpacked = res.tryUnpack(constraint.owner); unpacked && unpacked->getTableIndex() == table.tableID.getTableIndex()) {
            const ConstraintStorage& storage = table.storage->at(unpacked->getElementIndex());
            //Ensure the owner is still pointing at this constraint, could either be cleared or a newer one
            if(constraint.storage == storage) {
              //Ensure the members of the constraint exist
              auto [a, b] = getEdge(*graph, constraint.storage.getHandle());
              //A must exist, B is allowed to be unset but not invalid
              if(a && b.isUnsetOrValid()) {
                ++i;
                continue;
              }
            }
          }

          //If we made it here the entry is invalid, remove it
          table.changes->ownedEdges.erase(constraint.storage.getIndex());
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
      , targetB{ std::visit(ResolveConstTarget{ task, table }, definition.targetB) }
      //, custom{ getOrAssert(definition.common.read(), task, table) }
      , stableA{ &task.query<const StableIDRow>(table).get<0>(0) }
      , storage{ getOrAssert(definition.storage, task, table) }
      , joints{ getOrAssert(definition.joint.read(), task, table) }
    {
      if (definition.custom) {
        custom = getOrAssert(definition.custom.read(), task, table);
      }
    }

    Rows::ConstTarget targetA, targetB;
    const CustomConstraintRow* custom{};
    const StableIDRow* stableA{};
    const ConstraintStorageRow* storage{};
    const JointRow* joints{};
  };

  struct ConfigureJoint {
    void operator()(DisabledJoint) {
      manifold->setEnd(0);
    }

    void operator()(const CustomJoint&) const {
      if(table.custom) {
        //Copy as-is, preserving warm starts in manifold
        const Constraints::Constraint3DOF& source = table.custom->at(i);
        const int sourceSize = source.size();
        for(int c = 0; c < sourceSize; ++c) {
          manifold->sideA[c] = source.sideA[c];
          manifold->sideB[c] = source.sideB[c];
          float temp = manifold->common[c].warmStart;
          manifold->common[c] = source.common[c];
          manifold->common[c].warmStart = temp;
        }
        manifold->setEnd(sourceSize);
      }
      else {
        assert(false && "Custom joints must be used with a CustomConstraintRow");
      }
    }

    void operator()(const PinJoint1D& pin) const {
      const pt::Transform ta = transform.resolve(a);
      const pt::Transform tb = transform.resolve(b);
      const glm::vec2 aToPin = ta.transformVector(pin.localCenterToPinA);
      const glm::vec2 worldA = aToPin + ta.pos;
      const glm::vec2 bToPin = tb.transformVector(pin.localCenterToPinB);
      const glm::vec2 worldB = bToPin + tb.pos;
      glm::vec2 axis = worldA - worldB;
      const float error = glm::length(axis);
      if(error < Geo::EPSILON) {
        // Arbitrary axis 
        axis = glm::vec2{ 1, 0 };
      }
      else {
        axis /= error;
      }
      manifold->sideA[0].linear = axis;
      manifold->sideA[0].angular = Geo::cross(aToPin, manifold->sideA[0].linear);
      manifold->sideB[0].linear = -axis;
      manifold->sideB[0].angular = Geo::cross(bToPin, manifold->sideB[0].linear);
      const float bias = Geo::reduce(error - pin.distance, *globals.slop);
      manifold->common[0].bias = -bias**globals.biasTerm;
      manifold->common[0].lambdaMin = std::numeric_limits<float>::lowest();
      manifold->common[0].lambdaMax = std::numeric_limits<float>::max();
      manifold->setEnd(1);
    }

    void operator()(const PinJoint2D& pin) const {
      const pt::Transform ta = transform.resolve(a);
      const pt::Transform tb = transform.resolve(b);
      const glm::vec2 aToPin = ta.transformVector(pin.localCenterToPinA);
      const glm::vec2 worldA = aToPin + ta.pos;
      const glm::vec2 bToPin = tb.transformVector(pin.localCenterToPinB);
      const glm::vec2 worldB = bToPin + tb.pos;
      glm::vec2 axis = worldA - worldB;
      float distance = glm::length(axis);
      //Avoid division by zero below with an arbitrary axis if it is zero
      if(distance < Geo::EPSILON) {
        axis = { 1, 0 };
        distance = 1;
      }
      //Error is computed as the distance between the two points but they are solved separately along the x and y axis
      //Stretch the axis between the points to the target distance and use that to distribute the error between the axes
      const float error = Geo::reduce(pin.distance - distance, *globals.slop);
      axis *= error/distance;

      manifold->sideA[0].linear = { 1, 0 };
      manifold->sideA[0].angular = Geo::crossXAxis(aToPin, 1);
      manifold->sideB[0].linear = { -1, 0 };
      manifold->sideB[0].angular = Geo::crossXAxis(bToPin, -1);

      manifold->sideA[1].linear = { 0, 1 };
      manifold->sideA[1].angular = Geo::crossYAxis(aToPin, 1);
      manifold->sideB[1].linear = { 0, -1 };
      manifold->sideB[1].angular = Geo::crossYAxis(bToPin, -1);

      manifold->common[0].bias = axis.x**globals.biasTerm;
      manifold->common[0].lambdaMin = std::numeric_limits<float>::lowest();
      manifold->common[0].lambdaMax = std::numeric_limits<float>::max();

      manifold->common[1].bias = axis.y**globals.biasTerm;
      manifold->common[1].lambdaMin = std::numeric_limits<float>::lowest();
      manifold->common[1].lambdaMax = std::numeric_limits<float>::max();

      manifold->setEnd(2);
    }

    void operator()(const WeldJoint& pin) const {
      const pt::Transform ta = transform.resolve(a);
      const pt::Transform tb = transform.resolve(b);
      const glm::vec2 aToPin = ta.transformVector(pin.localCenterToPinA);
      const glm::vec2 worldA = aToPin + ta.pos;
      const glm::vec2 bToPin = tb.transformVector(pin.localCenterToPinB);
      const glm::vec2 worldB = bToPin + tb.pos;
      glm::vec2 axis = worldA - worldB;
      float distance = glm::length(axis);
      //Avoid division by zero below with an arbitrary axis if it is zero
      if(distance < Geo::EPSILON) {
        axis = { 1, 0 };
        distance = 1;
      }
      //Error is computed as the distance between the two points but they are solved separately along the x and y axis
      //Stretch the axis between the points to the target distance and use that to distribute the error between the axes
      const float linearError = Geo::reduce(distance, *globals.slop);
      axis *= -linearError/distance;

      //Anchors are expected to be pointing each-other completely like --><-- meaning that dot product of -1 would be zero error
      const glm::vec2 referenceA = glm::normalize(aToPin);
      //Flip so ideal cos is 1 rather than -1
      const glm::vec2 referenceB = glm::normalize(-bToPin);
      const float cosAngle = glm::dot(referenceA, referenceB);
      const float sinAngle = Geo::cross(aToPin, bToPin);
      const float angularErrorAbs = cosAngle > 0.0f ? std::acos(cosAngle) : std::acos(-cosAngle) + Geo::PI2;
      float angularError = Geo::reduce(angularErrorAbs, *globals.slop);
      if(sinAngle > 0) {
        angularError = -angularError;
      }

      manifold->sideA[0].linear = { 1, 0 };
      manifold->sideA[0].angular = Geo::crossXAxis(aToPin, 1);
      manifold->sideB[0].linear = { -1, 0 };
      manifold->sideB[0].angular = Geo::crossXAxis(bToPin, -1);

      manifold->sideA[1].linear = { 0, 1 };
      manifold->sideA[1].angular = Geo::crossYAxis(aToPin, 1);
      manifold->sideB[1].linear = { 0, -1 };
      manifold->sideB[1].angular = Geo::crossYAxis(bToPin, -1);

      //TODO: would be more stable to gradually get stiffer rather than all or nothing like this
      if(angularErrorAbs > pin.allowedRotationRad) {
        manifold->sideA[2].linear = {};
        manifold->sideA[2].angular = 1;
        manifold->sideB[2].linear = {};
        manifold->sideB[2].angular = -1;

        manifold->common[2].bias = angularError**globals.biasTerm;
        manifold->common[2].lambdaMin = std::numeric_limits<float>::lowest();
        manifold->common[2].lambdaMax = std::numeric_limits<float>::max();
      }
      else {
        manifold->setEnd(2);
      }

      manifold->common[0].bias = axis.x**globals.biasTerm;
      manifold->common[0].lambdaMin = std::numeric_limits<float>::lowest();
      manifold->common[0].lambdaMax = std::numeric_limits<float>::max();

      manifold->common[1].bias = axis.y**globals.biasTerm;
      manifold->common[1].lambdaMin = std::numeric_limits<float>::lowest();
      manifold->common[1].lambdaMax = std::numeric_limits<float>::max();
    }

    void operator()(const MotorJoint& pin) const {
      if(!pin.angularForce && !pin.linearForce && !pin.zForce) {
        manifold->setEnd(0);
        zManifold->clear();
        return;
      }
      //For now solving as a one-sided constraint until I see a use case for a two sided motor
      const pt::Transform ta = transform.resolve(a);
      size_t c = 0;

      if(pin.linearForce) {
        glm::vec2 linear = pin.linearTarget;
        if(!pin.flags.test(gnx::enumCast(MotorJoint::Flags::WorldSpaceLinear))) {
          linear = ta.transformVector(linear);
        }

        const float linearLen = glm::length(linear);
        if(linearLen > Geo::EPSILON) {
          manifold->sideA[c].linear = linear/linearLen;
          manifold->sideA[c].angular = 0;
          manifold->common[c].bias = linearLen;
          manifold->common[c].lambdaMax = pin.linearForce;
          manifold->common[c].lambdaMin = pin.flags.test(gnx::enumCast(MotorJoint::Flags::CanPull)) ? -pin.linearForce : 0.0f;
          ++c;
        }
        else {
          //If a force is given but no direction, solve towards zero velocity
          for(auto axis : { glm::vec2{ 1, 0 }, glm::vec2{ 0, 1 } }) {
            manifold->sideA[c].linear = axis;
            manifold->sideA[c].angular = 0;
            manifold->common[c].bias = 0;
            //Checking direction when pulling towards zero doesn't make sense so this ignores CanPull
            manifold->common[c].lambdaMax = pin.linearForce;
            manifold->common[c].lambdaMin = -pin.linearForce;
            ++c;
          }
        }
      }

      if(pin.angularForce) {
        manifold->sideA[c].linear = {};
        manifold->sideA[c].angular = 1;
        manifold->common[c].lambdaMax = pin.angularForce;
        manifold->common[c].lambdaMin = pin.flags.test(gnx::enumCast(MotorJoint::Flags::CanPull)) ? -pin.angularForce : 0.0f;

        //Try to point at target orientation
        if(pin.flags.test(gnx::enumCast(MotorJoint::Flags::AngularOrientationTarget))) {
          const float error = computeAngularError(Geo::directionFromAngle(pin.angularTarget), ta.rot);
          manifold->common[c].bias = error*pin.biasScalar;
        }
        //Absolute rotation in a given direction
        else {
          manifold->common[c].bias = pin.angularTarget;
        }
        ++c;
      }

      if(pin.zForce) {
        zManifold->common.bias = pin.linearTargetZ;
        zManifold->common.lambdaMax = pin.zForce;
        zManifold->common.lambdaMin = pin.flags.test(gnx::enumCast(MotorJoint::Flags::CanPull)) ? -pin.zForce : 0.0f;
        *pairType = c ? SP::PairType::ConstraintWithZ : SP::PairType::ConstraintZOnly;
      }
      else {
        zManifold->clear();
        *pairType = SP::PairType::Constraint;
      }

      manifold->setEnd(c);
    }

    void operator()(const PinMotorJoint& pin) const {
      if(!pin.force && !pin.orthogonalForce) {
        manifold->setEnd(0);
        return;
      }
      const pt::Transform ta = transform.resolve(a);
      const glm::vec2 worldCenterToPinA = ta.transformVector(pin.localCenterToPinA);
      glm::vec2 worldMotorDir = pin.targetVelocity;
      const float targetSpeed = glm::length(worldMotorDir);
      if(targetSpeed > Geo::EPSILON) {
        worldMotorDir /= targetSpeed;
      }

      int c = 0;
      //Impulse along main direction
      manifold->sideA[c].linear = worldMotorDir;
      manifold->sideA[c].angular = Geo::cross(worldCenterToPinA, worldMotorDir);
      manifold->common[c].bias = targetSpeed;
      manifold->common[c].lambdaMax = pin.force;
      manifold->common[c].lambdaMin = -pin.force;
      ++c;

      if(pin.orthogonalForce) {
        //Target zero velocity along the orthogonal to limit swinging past the target
        manifold->sideA[1].linear = Geo::orthogonal(worldMotorDir);
        manifold->sideA[1].angular = Geo::cross(worldCenterToPinA, manifold->sideA[1].linear);
        manifold->common[1].lambdaMax = pin.orthogonalForce;
        manifold->common[1].lambdaMin = -pin.orthogonalForce;
        ++c;
      }

      manifold->setEnd(c);
    }

    float biasFromError(float error, float biasTerm) const {
      return Geo::reduce(error, *globals.slop)*biasTerm;
    }

    float biasFromError(float error) const {
      return biasFromError(error, *globals.biasTerm);
    }

    float computeAngularError(const glm::vec2& referenceA, const glm::vec2& referenceB) const {
      const float cosAngle = glm::dot(referenceA, referenceB);
      const float sinAngle = Geo::cross(referenceA, referenceB);
      const float angularErrorAbs = cosAngle > 0.0f ? std::acos(cosAngle) : std::acos(-cosAngle) + Geo::PI2;
      float angularError = Geo::reduce(angularErrorAbs, *globals.slop);
      if(sinAngle > 0) {
        angularError = -angularError;
      }
      return angularError;
    }

    const ConstraintTable& table;
    SP::ConstraintManifold* manifold{};
    SP::ZConstraintManifold* zManifold{};
    SP::PairType* pairType{};
    pt::TransformResolver& transform;
    size_t i{};
    //TODO: optimize for self target lookup
    ElementRef a, b;
    const ConstraintSolver::SolverGlobals& globals;
  };

  struct SpatialPairsTable {
    SP::ConstraintRow* manifold{};
    SP::ZConstraintRow* zManifold{};
    SP::PairTypeRow* pairType{};
  };
  struct ConfigureArgs {
    const ConstraintTable& table;
    SpatialPairsTable& spatialPairs;
    const IslandGraph::Graph& graph;
    pt::TransformResolver& transformResolver;
    const ConstraintSolver::SolverGlobals& globals;
  };

  //Copies the constraint information as configured by gameplay over to the SpatialPairsStorage
  void configureTable(ConfigureArgs& args) {
    if(std::holds_alternative<NoTarget>(args.table.targetA)) {
      assert(false && "side A must always point at something");
      return;
    }
    ConfigureJoint joint{
      .table{ args.table },
      .manifold{ nullptr },
      .transform{ args.transformResolver },
      .globals{ args.globals }
    };

    for(size_t i = 0; i < args.table.storage->size(); ++i) {
      const ConstraintStorage& storage = args.table.storage->at(i);
      if(!storage.isValid()) {
        continue;
      }
      const auto edge = args.graph.findEdge(storage.getHandle());
      if(edge == args.graph.cEdgesEnd()) {
        continue;
      }
      SP::ConstraintManifold& manifold = args.spatialPairs.manifold->at(storage.getIndex());
      SP::ZConstraintManifold& zManifold = args.spatialPairs.zManifold->at(storage.getIndex());
      const auto [a, b] = edge.getNodes();

      //Solve if target is self or target is a valid external target
      bool shouldSolve = static_cast<bool>(a);

      if(shouldSolve) {
        joint.manifold = &manifold;
        joint.zManifold = &zManifold;
        joint.pairType = &args.spatialPairs.pairType->at(storage.getIndex());
        //Default to XY constraint, visitor can add Z
        *joint.pairType = SP::PairType::Constraint;
        joint.a = a;
        joint.b = b;
        joint.i = i;
        std::visit(joint, args.table.joints->at(i).data);
      }
      else {
        manifold.setEnd(0);
      }
    }
  }

  void constraintNarrowphase(IAppBuilder& builder, const PhysicsAliases& aliases, const ConstraintSolver::SolverGlobals& globals) {
    auto task = builder.createTask();
    std::vector<ConstraintTable> constraintTables = queryConstraintTables<ConstraintTable>(task);
    auto sp = task.query<const SP::IslandGraphRow, SP::PairTypeRow, SP::ConstraintRow, SP::ZConstraintRow>();
    assert(sp.size());
    auto [g, p, c, cz] = sp.get(0);
    SpatialPairsTable spatialPairs{ c.get(), cz.get(), p.get() };
    const IslandGraph::Graph* graph = &g->at();
    pt::TransformResolver tr{ task, aliases };

    task.setCallback([constraintTables, spatialPairs, graph, tr, globals](AppTaskArgs&) mutable {
      //TODO: multithreaded task
      for(const ConstraintTable& table : constraintTables) {
        ConfigureArgs cargs{
          .table{ table },
          .spatialPairs{ spatialPairs },
          .graph{ *graph },
          .transformResolver{ tr },
          .globals{ globals }
        };
        configureTable(cargs);
      }
    });

    builder.submitTask(std::move(task.setName("constraint narrowphase")));
  }

  void update(IAppBuilder& builder, const PhysicsAliases& aliases, const ConstraintSolver::SolverGlobals& globals) {
    Constraints::garbageCollect(builder);
    Constraints::assignStorage(builder);
    Constraints::constraintNarrowphase(builder, aliases, globals);
  }

  //ResolveTarget resolves a definition into the row variant. This takes the row variant and resolves to a concrete ElementRef
  struct ResolveTargetElement {
    ElementRef operator()(NoTarget) const {
      return {};
    }

    ElementRef operator()(const ExternalTargetRow* row) const {
      return row->at(self.getMapping()->getElementIndex()).target;
    }

    ElementRef operator()(SelfTarget) const {
      return self;
    }

    const ElementRef& self;
  };

  //Use the constraint definition to assign constraint storage for newly created elements that have AutoManageJointTag
  //The definition is used to determine what the constraint targets are
  struct AutoInitInternalJoints {
    struct Managed {
      struct Definition {
        //Modifier to create the storage
        std::shared_ptr<Constraints::IConstraintStorageModifier> modifier;
        //Rows to be able to resolve the target
        Rows::ConstTarget targetA, targetB;
      };
      std::vector<Definition> definitions;
    };

    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
      managedTables.resize(query.size());

      for(size_t i = 0; i < query.size(); ++i) {
        const auto& [def, x, y, z] = query.get(i);
        const TableID& table = query[i];
        Managed& managed = managedTables[i];
        ResolveConstTarget resolveTarget{ task, table };
        for(size_t c = 0; c < def->at().definitions.size(); ++c) {
          const Definition& definition = def->at().definitions[c];
          managed.definitions.push_back(Managed::Definition{
            .modifier = Constraints::createConstraintStorageModifier(task, c, table),
            .targetA = std::visit(resolveTarget, definition.targetA),
            .targetB = std::visit(resolveTarget, definition.targetB),
          });
        }
      }
    }

    void execute() {
      //If any elements are created in a tracked table, inform the constraint modifiers
      //An equivalent removal is not necessary as GC will see it
      for(size_t t = 0; t < query.size(); ++t) {
        auto [defs, tag, events, stable] = query.get(t);
        for(auto event : *events) {
          //TODO: isn't this also relevant if an element moved here?
          if(event.second.isCreate()) {
            const size_t si = event.first;
            ResolveTargetElement resolveElement{ stable->at(si) };
            for(const Managed::Definition& def : managedTables[t].definitions) {
              const ElementRef a = std::visit(resolveElement, def.targetA);
              const ElementRef b = std::visit(resolveElement, def.targetB);
              def.modifier->insert(si, a, b);
            }
          }
        }
      }
    }

    QueryResult<const TableConstraintDefinitionsRow,
      const AutoManageJointTag,
      const Events::EventsRow,
      const StableIDRow
    > query;
    std::vector<Managed> managedTables;
  };

  void postProcessEvents(IAppBuilder& builder) {
    builder.submitTask(std::move(TLSTask::create<AutoInitInternalJoints>("auto init joints")));
  }
}