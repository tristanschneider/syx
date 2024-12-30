#include "Precompile.h"
#include "stat/ConstraintStatEffect.h"

#include "AppBuilder.h"
#include "AllStatEffects.h"

namespace ConstraintStatEffect {
  //There is only one in this table at index zero
  constexpr Constraints::ConstraintDefinitionKey STAT_KEY = 0;

  RuntimeTable& getArgs(AppTaskArgs& args) {
    return StatEffectDatabase::getStatTable<ConstraintStatEffect::TargetA>(args);
  }

  Constraints::Rows extractRows(AppTaskArgs& args) {
    RuntimeTable& table = getArgs(args);
    return {
      .targetA{ Constraints::Rows::Target{ table.tryGet<TargetA>() } },
      .targetB{ Constraints::Rows::Target{ table.tryGet<TargetB>() } },
      .custom{ table.tryGet<CustomConstraint>() },
      .joint{ table.tryGet<JointRow>() }
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
    auto task = builder.createTask();
    auto q = task.query<Constraints::TableConstraintDefinitionsRow, TargetA>();
    task.setCallback([q](AppTaskArgs&) mutable {
      for(size_t t = 0; t < q.size(); ++t) {
        auto [definitions, _] = q.get(t);
        Constraints::Definition def;
        def.custom = def.custom.create<CustomConstraint>();
        def.targetA = Constraints::ExternalTargetRowAlias::create<TargetA>();
        def.targetB = Constraints::ExternalTargetRowAlias::create<TargetB>();
        def.joint = Constraints::JointRowAlias::create<ConstraintStatEffect::JointRow>();
        def.storage = def.storage.create<ConstraintStatEffect::StorageRow>();
        definitions->at().definitions.push_back(def);
      }
    });
    builder.submitTask(std::move(task.setName("init constraint stat")));
  }

  void initStat(IAppBuilder& builder) {
    configureDefinition(builder);
  }

  void processStat(IAppBuilder& builder) {
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