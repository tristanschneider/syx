#include "Precompile.h"
#include "FragmentStateMachine.h"

#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"

namespace FragmentStateMachine {
  void setState(FragmentState& current, FragmentState::Variant&& desired) {
    std::lock_guard<std::mutex> lock{ current.desiredStateMutex };
    current.desiredState = std::move(desired);
  }

  std::optional<FragmentState::Variant> tryTakeDesiredState(FragmentState& current) {
    std::lock_guard<std::mutex> lock{ current.desiredStateMutex };
    return current.currentState.index() != current.desiredState.index() ? std::make_optional(current.desiredState) : std::nullopt;
  }

  struct EnterArgs {};
  struct UpdateArgs {};
  struct ExitArgs {};

  struct Visitor {
    EnterArgs enter;
    UpdateArgs update;
    ExitArgs exit;
  };

  struct IdleVisitor : Visitor {
    void onEnter(Idle&) {}
    void onUpdate(Idle&) {}
    void onExit(Idle&) {}
  };

  struct WanderVisitor : Visitor {
    void onEnter(Wander&) {}
    void onUpdate(Wander&) {}
    void onExit(Wander&) {}
  };

  struct StunnedVisitor : Visitor {
    void onEnter(Stunned&) {}
    void onUpdate(Stunned&) {}
    void onExit(Stunned&) {}
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

  template<class... Visitors>
  struct CombinedVisitor {
    template<class T>
    using VisitorFor = VisitorDetails::MatchingVisitorT<T, Visitors...>;

    struct EnterVisitor : Visitor {
      template<class T>
      void operator()(T& t) {
        VisitorFor<T> v;
        v.enter = enter;
        v.onEnter(t);
      }
    };

    struct UpdateVisitor : Visitor {
      template<class T>
      void operator()(T& t) {
        VisitorFor<T> v;
        v.update = update;
        v.onUpdate(t);
      }
    };

    struct ExitVisitor : Visitor {
      template<class T>
      void operator()(T& t) {
        VisitorFor<T> v;
        v.exit = exit;
        v.onExit(t);
      }
    };
  };

  using FragmentVisitor = CombinedVisitor<
    IdleVisitor,
    StunnedVisitor,
    WanderVisitor
  >;

  void updateState(StateRow& row, GameDB db, ThreadLocalData& tls) {
    db;tls;
    FragmentVisitor::EnterVisitor enterVisit;
    FragmentVisitor::UpdateVisitor updateVisit;
    FragmentVisitor::ExitVisitor exitVisit;

    for(size_t i = 0; i < row.size(); ++i) {
      FragmentState& state = row.at(i);
      //See if there is a desired state transition
      if(std::optional<FragmentState::Variant> desired = tryTakeDesiredState(state)) {
        //Exit the old state
        exitVisit.exit = {};
        std::visit(exitVisit, state.currentState);
        enterVisit.enter = {};
        //Swap to new state and enter it
        state.currentState = std::move(*desired);
        std::visit(enterVisit, state.currentState);
      }
      else {
        updateVisit.update = {};
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
      Queries::viewEachRow(db.db, [&db, &tls](StateRow& states) {
        updateState(states, db, tls);
      });
    });
    return TaskBuilder::addEndSync(root);
  }
}