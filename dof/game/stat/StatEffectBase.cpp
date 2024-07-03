#include "Precompile.h"
#include "stat/StatEffectBase.h"

#include "TableAdapters.h"
#include "AppBuilder.h"
#include "curve/CurveSolver.h"

namespace StatEffect {
  //Each set of commands must begin with this, creates the range of effects in this table
  BuilderBase& BuilderBase::createStatEffects(size_t count) {
    return currentEffects = gnx::makeIndexRangeBeginCount(modifier.addElements(count, nullptr), count), *this;
  }

  BuilderBase& BuilderBase::setLifetime(size_t value) {
    for(size_t i : currentEffects) {
      lifetime->at(i) = value;
    }
    return *this;
  }

  BuilderBase& BuilderBase::setOwner(size_t stableID) {
    const auto id = StableElementID::fromStableID(stableID);
    for(size_t i : currentEffects) {
      owner->at(i) = id;
    }
    return *this;
  }

  void tickLifetime(IAppBuilder* builder, const UnpackedDatabaseElementID& table, size_t removeOnTick) {
    auto task = builder->createTask();
    task.setName("tick stat lifetime");
    auto query = task.query<Global, Lifetime, const StableIDRow>(table);

    task.setCallback([query, removeOnTick](AppTaskArgs&) mutable {
      auto&& [global, lifetime, stableRow] = query.get(0);
      for(size_t i = 0; i < lifetime->size(); ++i) {
        size_t& remaining = lifetime->at(i);
        if(remaining > removeOnTick) {
          if(remaining != INFINITE) {
            --remaining;
          }
        }
        else {
          //Let the removal processing resolve the unstable id, set invalid here
          global->at().toRemove.push_back(StableElementID::fromStableRow(i, *stableRow));
        }
      }
    });

    builder->submitTask(std::move(task));
  }

  void processRemovals(IAppBuilder& builder, const UnpackedDatabaseElementID& table) {
    auto task = builder.createTask();
    task.setName("remove stats");
    auto ids = task.getIDResolver();
    auto query = task.query<Global>(table);
    auto modifier = task.getModifierForTable(table);

    task.setCallback([ids, query, modifier](AppTaskArgs&) mutable {
      std::vector<StableElementID>& toRemove = query.tryGetSingletonElement()->toRemove;
      for(const StableElementID& id : toRemove) {
        if(auto resolved = ids->tryResolveAndUnpack(id)) {
          modifier->swapRemove(resolved->unpacked);
        }
      }

      toRemove.clear();
    });

    builder.submitTask(std::move(task));
  }

  void processCompletionContinuation(IAppBuilder& builder, const UnpackedDatabaseElementID& table) {
    auto task = builder.createTask();
    task.setName("stat continuation");
    auto ids = task.getIDResolver();
    auto query = task.query<
      Continuations,
      Global
    >(table);
    RuntimeDatabase& db = task.getDatabase();

    task.setCallback([ids, query, &db](AppTaskArgs& taskArgs) mutable {
      auto [continuation, global] = query.get(0);
      for(StableElementID& id : global->at().toRemove) {
        if(auto resolved = ids->tryResolveAndUnpack(id)) {
          //Store the resolved id for processRemovals
          id = resolved->stable;

          //Find and call the continuation, if any
          Continuation c = std::move(continuation->at(resolved->unpacked.getElementIndex()));
          if(!c.onComplete.empty()) {
            Continuation::Callback fn{ std::move(c.onComplete.front()) };
            c.onComplete.pop_front();

            Continuation::Args args {
              db,
              resolved->unpacked,
              taskArgs,
              std::move(c)
            };

            fn(args);
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void resolveOwners(IAppBuilder& builder, const UnpackedDatabaseElementID& table) {
    auto task = builder.createTask();
    task.setName("resolve owners");
    auto query = task.query<StatEffect::Owner>(table);
    auto ids = task.getIDResolver();
    task.setCallback([query, ids](AppTaskArgs&) mutable {
      for(StableElementID& toResolve : query.getSingleton<0>()->mElements) {
        toResolve = ids->tryResolveStableID(toResolve).value_or(StableElementID::invalid());
      }
    });
    builder.submitTask(std::move(task));
  }

  void resolveTargets(IAppBuilder& builder, const UnpackedDatabaseElementID& table) {
    if(!builder.queryTable<StatEffect::Target>(table)) {
      return;
    }
    auto task = builder.createTask();
    task.setName("resolve targets");
    auto query = task.query<StatEffect::Target>(table);
    auto ids = task.getIDResolver();
    task.setCallback([query, ids](AppTaskArgs&) mutable {
      for(StableElementID& toResolve : query.getSingleton<0>()->mElements) {
        toResolve = ids->tryResolveStableID(toResolve).value_or(StableElementID::invalid());
      }
    });
    builder.submitTask(std::move(task));
  }

  void solveCurves(IAppBuilder& builder, const UnpackedDatabaseElementID& table, const CurveAlias& alias) {
    {
      auto task = builder.createTask();
      task.setName("advance curve time");
      auto query = task.queryAlias(table, alias.curveIn, alias.curveDef.read());
      const float* dt = TableAdapters::getDeltaTime(task);
      if(!query.size()) {
        task.discard();
        return;
      }

      task.setCallback([query, dt](AppTaskArgs&) mutable {
        auto&& [curveInput, definition] = query.get(0);
        for(size_t i = 0; i < curveInput->size(); ++i) {
          //Another possibility would be scanning forward to look for matching definitions so they can be solved in groups
          CurveSolver::CurveUniforms uniforms{ 1 };
          //Update input time in place
          CurveSolver::CurveVaryings varyings{ &curveInput->at(i), &curveInput->at(i) };
          CurveSolver::advanceTime(*definition->at(i), uniforms, varyings, *dt);
        }
      });

      builder.submitTask(std::move(task));
    }
    {
      auto task = builder.createTask();
      task.setName("solvecurves");
      auto query = task.queryAlias(alias.curveIn.read(), alias.curveOut, alias.curveDef.read());
      if(!query.size()) {
        task.discard();
        return;
      }

      task.setCallback([query](AppTaskArgs&) mutable {
        auto&& [curveInput, curveOutput, definition] = query.get(0);
        for(size_t i = 0; i < curveInput->size(); ++i) {
          CurveSolver::CurveUniforms uniforms{ 1 };
          CurveSolver::CurveVaryings varyings{ &curveInput->at(i), &curveOutput->at(i) };
          CurveSolver::solve(*definition->at(i), uniforms, varyings);
        }
      });

      builder.submitTask(std::move(task));
    }
  }
}