#include "Precompile.h"
#include "stat/ConstraintStatEffect.h"

#include "AppBuilder.h"
#include "AllStatEffects.h"

namespace ConstraintStatEffect {
  //There is only one in this table at index zero
  constexpr Constraints::ConstraintDefinitionKey STAT_KEY = 0;

  auto getArgs(AppTaskArgs& args) {
    return StatEffectDatabase::createBuilderBase<ConstraintStatEffectTable>(args);
  }

  Constraints::Rows extractRows(AppTaskArgs& args) {
    ConstraintStatEffectTable& table = getArgs(args).table;
    return {
      Constraints::Rows::Target{ &std::get<TargetA>(table.mRows) },
      Constraints::Rows::Target{ &std::get<TargetB>(table.mRows) },
      &std::get<ConstraintA>(table.mRows),
      &std::get<ConstraintB>(table.mRows),
      &std::get<ConstraintCommon>(table.mRows)
    };
  }

  Builder::Builder(AppTaskArgs& args)
    : BuilderBase{ getArgs(args) }
    , builder{ extractRows(args) }
  {
  }

  Constraints::Builder& Builder::constraintBuilder() {
    return builder.select(currentEffects);
  }

  void markExpiredForDeletion(const Constraints::ExternalTargetRow& ids, StatEffect::Lifetime& lifetime) {
    for(size_t i = 0; i < ids.size(); ++i) {
      if(const ElementRef& ref = ids.at(i).target; ref.isSet() && !ref) {
        lifetime.at(i) = 0;
      }
    }
  }

  //Lets physics system know how to find this
  void configureDefinition(IAppBuilder& builder) {
    auto temp = builder.createTask();
    temp.discard();
    auto q = temp.query<Constraints::TableConstraintDefinitionsRow, TargetA>();
    for(size_t t = 0; t < q.size(); ++t) {
      auto [definitions, _] = q.get(t);
      Constraints::Definition def;
      def.common = def.common.create<ConstraintCommon>();
      def.sideA = def.sideA.create<ConstraintA>();
      def.sideB = def.sideB.create<ConstraintB>();
      def.targetA = Constraints::ExternalTargetRowAlias::create<TargetA>();
      def.targetB = Constraints::ExternalTargetRowAlias::create<TargetB>();
      definitions->at().definitions.push_back(def);
    }
  }

  void processStat(IAppBuilder& builder) {
    configureDefinition(builder);

    auto task = builder.createTask();
    task.setName("constraint stat");
    auto q = task.query<
      const StatEffect::Global,
      const TargetA,
      const TargetB,
      TickTrackerRow,
      StatEffect::Lifetime
    >();
    std::vector<std::shared_ptr<Constraints::IConstraintStorageModifier>> modifiers;
    modifiers.reserve(q.size());
    for(size_t i = 0; i < q.size(); ++i) {
      modifiers.push_back(Constraints::createConstraintStorageModifier(task, STAT_KEY, q.matchingTableIDs[i]));
    }
    ElementRefResolver res = task.getIDResolver()->getRefResolver();
    //Notify additions and removals
    //Occasionally check for expiration of targets
    task.setCallback([=](AppTaskArgs&) mutable {
      for(size_t t = 0; t < q.size(); ++t) {
        Constraints::IConstraintStorageModifier& modifier = *modifiers[t];
        auto&& [global, a, b, tick, lifetime] = q.get(t);
        for(const ElementRef& added : global->at().newlyAdded) {
          if(auto id = res.tryUnpack(added)) {
            const size_t i = id->getElementIndex();
            modifier.insert(i, a->at(i).target, b->at(i).target);
          }
        }
        for(const ElementRef& removed : global->at().toRemove) {
          if(auto id = res.tryUnpack(removed)) {
            const size_t i = id->getElementIndex();
            modifier.erase(i, a->at(i).target, b->at(i).target);
          }
        }

        //Occasionally sweep through and make sure targets exist. If they don't, mark for deletion by setting their lifetime to zero
        //Next tick they will then be removed
        if(tick->at().ticksSinceSweep++ > 100) {
          tick->at().ticksSinceSweep = 0;
          markExpiredForDeletion(*a, *lifetime);
          markExpiredForDeletion(*b, *lifetime);
        }
      }
    });
    builder.submitTask(std::move(task));
  }
}