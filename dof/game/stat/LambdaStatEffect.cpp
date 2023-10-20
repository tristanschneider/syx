#include "Precompile.h"
#include "stat/LambdaStatEffect.h"

#include "Simulation.h"
#include "TableAdapters.h"

namespace LambdaStatEffect {
  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("lambda stat");
    RuntimeDatabase& db = task.getDatabase();
    auto query = task.query<
      const StatEffect::Owner,
      const LambdaRow
    >();
    auto ids = task.getIDResolver();

    task.setCallback([query, &db, ids](AppTaskArgs&) mutable {
      Args args{};
      args.db = &db;
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [owner, lambda] = query.get(t);
        for(size_t i = 0; owner->size(); ++i) {
          //Normal stats could rely on resolving upfront before processing stat but since lambda has
          //access to the entire database it could cause table migrations
          auto resolved = ids->tryResolveStableID(owner->at(i));
          args.resolvedID = resolved.value_or(StableElementID::invalid());
          lambda->at(i)(args);
        }
      }
    });

    builder.submitTask(std::move(task));
  }
};
