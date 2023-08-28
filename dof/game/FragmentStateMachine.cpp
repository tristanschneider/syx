#include "Precompile.h"
#include "FragmentStateMachine.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "stat/FollowTargetByVelocityEffect.h"
#include "stat/LambdaStatEffect.h"

#include "glm/glm.hpp"
#include "glm/gtx/norm.hpp"

namespace FragmentStateMachine {
  bool tryResolve(StableElementID& id, const StableElementMappings& mappings) {
    if(auto resolved = StableOperations::tryResolveStableID(id, mappings)) {
      id = *resolved;
      return true;
    }
    return false;
  }

  std::optional<StableElementID> tryGetFirstNearby(const SpatialQuery::Result& results, const StableElementID& self) {
    auto it = std::find_if(results.nearbyObjects.begin(), results.nearbyObjects.end(), [&](const StableElementID& e) { return e.mStableID != self.mStableID; });
    return it != results.nearbyObjects.end() ? std::make_optional(*it) : std::nullopt;
  }

  void setState(FragmentState& current, FragmentState::Variant&& desired) {
    std::lock_guard<std::mutex> lock{ current.desiredStateMutex };
    current.desiredState = std::move(desired);
  }

  void setState(GameDB db, const StableElementID& id, FragmentState::Variant&& desired) {
    if(auto resolved = StableOperations::tryResolveStableID(id, TableAdapters::getStableMappings(db))) {
      const auto self = resolved->toPacked<GameDatabase>();
      if(auto* row = Queries::getRowInTable<StateRow>(db.db, self)) {
        setState(row->at(self.getElementIndex()), std::move(desired));
      }
    }
  }

  std::optional<FragmentState::Variant> tryTakeDesiredState(FragmentState& current) {
    std::lock_guard<std::mutex> lock{ current.desiredStateMutex };
    return current.currentState.index() != current.desiredState.index() ? std::make_optional(current.desiredState) : std::nullopt;
  }

  struct BaseArgs {
    UnpackedDatabaseElementID self;
    GameDB* db{};
    size_t thread{};
    ThreadLocalData* tls{};
    //Desired state of the fragment currently being visited
    FragmentState::Variant* desiredState{};
  };

  struct Visitor {
    BaseArgs args;
  };

  struct IdleVisitor : Visitor {
    using VX = FloatRow<Tags::GLinVel, Tags::X>;
    using VY = FloatRow<Tags::GLinVel, Tags::Y>;
    CachedRow<VX> velX;
    CachedRow<VY> velY;
    TableResolver<VX, VY, StateRow> resolver;

    void init() {
      resolver = resolver.create(args.db->db);
    }
    void onEnter(Idle&) {
    }
    void onUpdate(Idle&) {
      resolver.tryGetOrSwapRow(velX, args.self);
      resolver.tryGetOrSwapRow(velY, args.self);
      if(!velX || !velY) {
        return;
      }

      //"wake up" when nudged by something, and start wandering
      const size_t si = args.self.getElementIndex();
      constexpr float activationThreshold = 0.0001f;
      const float speed2 = glm::length2(TableAdapters::read(si, *velX, *velY));
      if(speed2 > activationThreshold) {
        *args.desiredState = Wander{};
      }
    }
    void onExit(Idle&) {}
  };

  struct WanderVisitor : Visitor {
    using LIX = FloatRow<Tags::GLinImpulse, Tags::X>;
    using LIY = FloatRow<Tags::GLinImpulse, Tags::Y>;
    using AV = FloatRow<Tags::GAngVel, Tags::Angle>;
    using AI = FloatRow<Tags::GAngImpulse, Tags::Angle>;
    using RX = FloatRow<Tags::GRot, Tags::CosAngle>;
    using RY = FloatRow<Tags::GRot, Tags::SinAngle>;
    using PX = FloatRow<Tags::GPos, Tags::X>;
    using PY = FloatRow<Tags::GPos, Tags::Y>;
    CachedRow<LIX> lix;
    CachedRow<LIY> liy;
    CachedRow<RX> rx;
    CachedRow<RY> ry;
    CachedRow<PX> px;
    CachedRow<PY> py;
    CachedRow<AI> angularImpulse;
    CachedRow<AV> angularVelocity;
    CachedRow<StableIDRow> stable;
    TableResolver<LIX, LIY, RX, RY, AI, StableIDRow, PX, PY, AV> resolver;
    SpatialQueryAdapter queries;
    StableElementMappings* mappings{};

    void init() {
      resolver = resolver.create(args.db->db);
      queries = TableAdapters::getSpatialQueries(*args.db);
      mappings = &TableAdapters::getStableMappings(*args.db);
    }

    void onEnter(Wander& wander) {
      wander.spatialQuery = SpatialQuery::createQuery(queries, { SpatialQuery::AABB{} }, 2);
    }

    void onUpdate(Wander& wander) {
      if(!resolver.tryGetOrSwapAllRows(args.self, lix, liy, rx, ry, px, py, stable, angularImpulse, angularVelocity)
        || !tryResolve(wander.spatialQuery, *mappings)) {
        return;
      }
      const size_t si = args.self.getElementIndex();
      const size_t stableID = stable->at(si);
      const StableElementID stableSelf{ args.self.mValue, stableID };
      const size_t qi = wander.spatialQuery.toPacked<GameDatabase>().getElementIndex();
      const SpatialQuery::Result& results = SpatialQuery::getResult(qi, queries);

      constexpr float linearSpeed = 0.003f;
      constexpr float angularAvoidance = 0.01f;
      constexpr float maxAngVel = 0.03f;
      constexpr float minAngVel = 0.001f;
      constexpr float angularDamping = 0.001f;
      constexpr float seekAhead = 2.0f;

      //Don't tinker with direction if already spinning out of control
      const float angVel = angularVelocity->at(si);
      const float angSpeed =  std::abs(angVel);
      if(angSpeed < maxAngVel) {
        //If something is in the way, turn to the right
        if(tryGetFirstNearby(results, stableSelf)) {
          angularImpulse->at(si) += angularAvoidance;
        }
      }
      if(angSpeed > minAngVel) {
        angularImpulse->at(si) += angVel * -angularDamping;
      }

      const glm::vec2 forward = TableAdapters::read(si, *rx, *ry);
      TableAdapters::add(si, forward * linearSpeed, *lix, *liy);

      //Put query volume a bit out in front of the fragment with size of 0.5
      const glm::vec2 pos = TableAdapters::read(si, *px, *py);
      const glm::vec2 queryPos = pos + forward*seekAhead;
      constexpr glm::vec2 half{ 0.25f, 0.25f };
      SpatialQuery::AABB bb{ queryPos - half, queryPos + half };
      SpatialQuery::refreshQuery(qi, queries, { bb }, 2);
    }
    //Cleanup of the query isn't needed because it'll expire after its lifetime
    void onExit(Wander&) {}
  };

  struct StunnedVisitor : Visitor {
    void init() {}
    void onEnter(Stunned&) {}
    void onUpdate(Stunned&) {}
    void onExit(Stunned&) {}
  };

  struct EmptyVisitor : Visitor {
    void init() {}
    void onEnter(Empty&) {}
    void onUpdate(Empty&) {}
    void onExit(Empty&) {}
  };

  struct SeekHomeVisitor : Visitor {
    TargetPosAdapter targets;
    FollowTargetByVelocityStatEffectAdapter followVelocity;
    CachedRow<StableIDRow> stable;
    CachedRow<Tint> tint;
    TableResolver<StableIDRow, Tint> resolver;
    LambdaStatEffectAdapter lambda;

    void init() {
      targets = TableAdapters::getTargetPos(*args.db);
      followVelocity = TableAdapters::getFollowTargetByVelocityEffects(*args.db, args.thread);
      lambda = TableAdapters::getLambdaEffects(*args.db, args.thread);
      resolver = resolver.create(args.db->db);
    }

    void onEnter(SeekHome& seek) {
      resolver.tryGetOrSwapRow(stable, args.self);
      resolver.tryGetOrSwapRow(tint, args.self);
      //Set color
      if(tint) {
        tint->at(args.self.getElementIndex()).r = 1.0f;
      }

      //Create target for follow behavior
      const size_t& stableId = stable->at(args.self.getElementIndex());
      const size_t newTarget = targets.modifier.addElements(1);
      StableElementID stableTarget = StableElementID::fromStableRow(newTarget, *targets.stable);
      seek.target = stableTarget;

      const size_t followTime = 100;
      const float springConstant = 0.001f;

      //Create the follow effect and point it at the target
      const size_t followEffect = TableAdapters::addStatEffectsSharedLifetime(followVelocity.base, followTime, &stableId, 1);
      followVelocity.command->at(followEffect).mode = FollowTargetByVelocityStatEffect::SpringFollow{ springConstant };
      followVelocity.base.target->at(followEffect) = stableTarget;
      //Enqueue the state transition back to wander
      followVelocity.base.continuations->at(followEffect).onComplete.push_back([stableId](StatEffect::Continuation::Args& a) {
        setState(a.db, StableElementID::fromStableID(stableId), { Wander{} });
      });
    }

    void onUpdate(SeekHome&) {
    }

    void onExit(SeekHome& seek) {
      //Reset color
      resolver.tryGetOrSwapRow(tint, args.self);
      if(tint) {
        tint->at(args.self.getElementIndex()).r = 0.0f;
      }

      StableElementID toRemove = seek.target;
      size_t effect = TableAdapters::addStatEffectsSharedLifetime(lambda.base, StatEffect::INSTANT, &toRemove.mStableID, 1);
      lambda.command->at(effect) = [](LambdaStatEffect::Args& a) {
        if(a.resolvedID != StableElementID::invalid()) {
          assert(a.resolvedID.toPacked<GameDatabase>().getTableIndex() == GameDatabase::getTableIndex<TargetPosTable>().getTableIndex());
          TableOperations::stableSwapRemove(std::get<TargetPosTable>(a.db->db.mTables), a.resolvedID.toPacked<GameDatabase>(), TableAdapters::getStableMappings(*a.db));
        }
      };
    }
  };

  namespace VisitorDetails {
    //Assume that if the visitor matches onEnter it matches them all
    template<class Visitor, class Value, class Enabled = void>
    struct DoesVisitorMatch : std::false_type {};
    template<class Visitor, class Value>
    struct DoesVisitorMatch<Visitor, Value, std::enable_if_t<
      std::is_same_v<
        void,
        decltype(std::declval<Visitor&>().onEnter(std::declval<Value&>()))
      >
    >> : std::true_type {};

    template<class Visitor, class... Visitors, class Value>
    constexpr auto getMatchingVisitor([[maybe_unused]] const Value& value) {
      //If this matches, return it
      if constexpr(DoesVisitorMatch<Visitor, Value>::value) {
        return Visitor{};
      }
      //If it doesn't match, recurse into the next one
      //No overload will be found if the last element isn't foun which is good
      //because it would mean a visitor is entirely missing from the template
      else {
        return getMatchingVisitor<Visitors...>(value);
      }
    }

    template<class Value, class... Visitors>
    using MatchingVisitorT = decltype(getMatchingVisitor<Visitors...>(std::declval<Value&>()));
  }

  //Mashes together a bunch of visitors by action (enter/exit) and creates a visitor that visits by behavior (idle/wander)
  template<class... Visitors>
  struct CombinedVisitor : Visitor {
    std::tuple<Visitors...> visitors;

    template<class T>
    auto& getVisitor() {
      return std::get<VisitorDetails::MatchingVisitorT<T, Visitors...>>(visitors);
    }

    template<class T>
    auto& getAndUpdateVisitor() {
      auto& v = getVisitor<T>();
      v.args.self = args.self;
      v.args.desiredState = args.desiredState;
      return v;
    }

    template<class T>
    void init(T& t) {
      t.args = args;
      t.init();
    }

    void init(GameDB& db, ThreadLocalData& tls, size_t thread) {
      args.db = &db;
      args.tls = &tls;
      args.thread = thread;
      (init(std::get<Visitors>(visitors)), ...);
    }

    auto getEnter() {
      return [this](auto& t) { onEnter(t); };
    }

    auto getUpdate() {
      return [this](auto& t) { onUpdate(t); };
    }

    auto getExit() {
      return [this](auto& t) { onExit(t); };
    }

    template<class T>
    void onEnter(T& t) {
      getAndUpdateVisitor<T>().onEnter(t);
    }

    template<class T>
    void onUpdate(T& t) {
      getAndUpdateVisitor<T>().onUpdate(t);
    }

    template<class T>
    void onExit(T& t) {
      getAndUpdateVisitor<T>().onExit(t);
    }
  };

  using FragmentVisitor = CombinedVisitor<
    IdleVisitor,
    StunnedVisitor,
    WanderVisitor,
    SeekHomeVisitor,
    EmptyVisitor
  >;
  struct UpdateArgs {
    size_t begin{};
    size_t end{};
    GameDatabase::ElementID tableId;
    StateRow& row;
    GameDB db;
    ThreadLocalData& tls;
    size_t thread{};
  };

  void updateState(UpdateArgs& args) {
    FragmentVisitor visitor;
    visitor.init(args.db, args.tls, args.thread);
    auto enterVisit = visitor.getEnter();
    auto updateVisit = visitor.getUpdate();
    auto exitVisit = visitor.getExit();

    for(size_t i = args.begin; i < args.end; ++i) {
      FragmentState& state = args.row.at(i);
      visitor.args.self = UnpackedDatabaseElementID::fromPacked(args.tableId).remakeElement(i);
      visitor.args.desiredState = &state.desiredState;

      //See if there is a desired state transition
      if(std::optional<FragmentState::Variant> desired = tryTakeDesiredState(state)) {
        //Exit the old state
        std::visit(exitVisit, state.currentState);
        //Swap to new state and enter it
        state.currentState = std::move(*desired);
        std::visit(enterVisit, state.currentState);
      }
      else {
        std::visit(updateVisit, state.currentState);
      }
    }
  }

  //Currently does synchronous iteration over each state
  //Optimization potential by doing a pass over them all and bucketing them into states,
  //execute in parallel
  TaskRange update(GameDB db) {
    auto root = TaskNode::create([db](enki::TaskSetPartition, uint32_t thread) {
      ThreadLocalData tls = TableAdapters::getThreadLocal(db, thread);
      Queries::viewEachRowWithTableID(db.db, [&db, &tls, thread](GameDatabase::ElementID id,
        StateRow& states) {
        UpdateArgs args {
          0, states.size(), id, states, db, tls, static_cast<size_t>(thread)
        };
        updateState(args);
      });
    });
    return TaskBuilder::addEndSync(root);
  }

  using EventResolver = TableResolver<
    StateRow
  >;
  void _processMigrations(const DBEvents& events, EventResolver resolver, GameDB db) {
    CachedRow<StateRow> fromState, toState;
    for(const DBEvents::MoveCommand& cmd : events.toBeMovedElements) {
      auto fromTable = cmd.source.toUnpacked<GameDatabase>();
      auto toTable =  cmd.destination.toUnpacked<GameDatabase>();
      resolver.tryGetOrSwapRow(fromState, fromTable);
      resolver.tryGetOrSwapRow(toState, toTable);
      //If a goal would be lost, trigger the exit callback by transitioning to the empty state
      if(fromState && !toState) {
        const size_t si = fromTable.getElementIndex();
        fromState->at(si).desiredState = Empty{};
        //Any is fine, these are scheduled synchronously
        const size_t thread = 0;
        auto tls = TableAdapters::getThreadLocal(db, thread);
        UpdateArgs args {
          si,
          si + 1,
          cmd.source.toPacked<GameDatabase>(),
          *fromState,
          db,
          tls,
          thread
        };
        updateState(args);
      }
    }
  }

  TaskRange preProcessEvents(GameDB db) {
    auto root = TaskNode::create([db](...) {
      auto resolver = EventResolver::create(db.db);
      const DBEvents& events = Events::getPublishedEvents(db);
      _processMigrations(events, resolver, db);
    });
    return TaskBuilder::addEndSync(root);
  }

}