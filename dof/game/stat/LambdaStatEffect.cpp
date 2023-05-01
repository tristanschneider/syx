#include "Precompile.h"
#include "stat/LambdaStatEffect.h"

#include "Simulation.h"
#include "TableAdapters.h"

namespace StatEffect {
  TaskRange processStat(const StatEffect::Owner& owner, LambdaStatEffect::LambdaRow& row, GameDB& db) {
    auto task = TaskNode::create([&owner, &row, db](...) mutable {
      LambdaStatEffect::Args args{};
      args.db = &db;
      StableElementMappings& mappings = TableAdapters::getStableMappings(db);
      for(size_t i = 0; i < row.size(); ++i) {
        const StableElementID unresolved = owner.at(i);
        //Normal stats could rely on resolving upfront before processing stat but since lambda has
        //access to the entire database it could cause table migrations
        if(auto resolved = StableOperations::tryResolveStableID(unresolved, db.db, mappings)) {
          args.resolvedID = *resolved;
          row.at(i)(args);
        }
      }
    });
    return TaskBuilder::addEndSync(task);
  }

  TaskRange processStat(LambdaStatEffectTable& table, GameDB db) {
    return processStat(std::get<StatEffect::Owner>(table.mRows), std::get<LambdaStatEffect::LambdaRow>(table.mRows), db);
  }
};
