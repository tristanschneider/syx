#include "Precompile.h"
#include "stat/LambdaStatEffect.h"

#include "AllStatEffects.h"
#include "TableAdapters.h"
#include "AppBuilder.h"

namespace LambdaStatEffect {
  auto getArgs(AppTaskArgs& args) {
    return StatEffectDatabase::createBuilderBase<LambdaStatEffectTable>(args);
  }

  Builder::Builder(AppTaskArgs& args)
    : BuilderBase(getArgs(args))
    , command{ &std::get<LambdaRow>(getArgs(args).table.mRows) }
  {
  }

  Builder& Builder::setLambda(const Lambda& l) {
    for(auto i : currentEffects) {
      command->at(i) = l;
    }
    return *this;
  }

  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("lambda stat");
    RuntimeDatabase& db = task.getDatabase();
    auto query = task.query<
      const StatEffect::Owner,
      const LambdaRow
    >();

    task.setCallback([query, &db](AppTaskArgs&) mutable {
      Args args{};
      args.db = &db;
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [owner, lambda] = query.get(t);
        for(size_t i = 0; i < owner->size(); ++i) {
          //Normal stats could rely on resolving upfront before processing stat but since lambda has
          //access to the entire database it could cause table migrations
          args.resolvedID = owner->at(i);
          lambda->at(i)(args);
        }
      }
    });

    builder.submitTask(std::move(task));
  }
};
