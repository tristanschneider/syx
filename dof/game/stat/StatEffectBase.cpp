#include "Precompile.h"
#include "stat/StatEffectBase.h"

#include "TableAdapters.h"
#include "AppBuilder.h"
#include "curve/CurveSolver.h"
#include "DBEvents.h"

namespace StatEffect {
  //Each set of commands must begin with this, creates the range of effects in this table
  BuilderBase& BuilderBase::createStatEffects(size_t count) {
    localDB.setTableDirty(table.getID());
    return currentEffects = gnx::makeIndexRangeBeginCount(table.addElements(count, nullptr), count), *this;
  }

  BuilderBase& BuilderBase::setLifetime(size_t value) {
    for(size_t i : currentEffects) {
      lifetime->at(i) = value;
    }
    return *this;
  }

  BuilderBase& BuilderBase::setOwner(const ElementRef& stableID) {
    for(size_t i : currentEffects) {
      owner->at(i) = stableID;
    }
    return *this;
  }

  void tickLifetime(IAppBuilder* builder, const TableID& table, size_t removeOnTick) {
    auto task = builder->createTask();
    task.setName("tick stat lifetime");
    auto query = task.query<Lifetime, const StableIDRow>(table);

    task.setCallback([query, removeOnTick](AppTaskArgs& args) mutable {
      auto&& [lifetime, stableRow] = query.get(0);
      for(size_t i = 0; i < lifetime->size(); ++i) {
        size_t& remaining = lifetime->at(i);
        if(remaining > removeOnTick) {
          if(remaining != INFINITE) {
            --remaining;
          }
        }
        else {
          Events::onRemovedElement(stableRow->at(i), args);
        }
      }
    });

    builder->submitTask(std::move(task));
  }

  void solveCurves(IAppBuilder& builder, const TableID& table, const CurveAlias& alias) {
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