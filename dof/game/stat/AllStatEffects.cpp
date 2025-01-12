#include "Precompile.h"
#include "stat/AllStatEffects.h"

#include "stat/AreaForceStatEffect.h"
#include "stat/ConstraintStatEffect.h"
#include "stat/DamageStatEffect.h"
#include "stat/FollowTargetByPositionEffect.h"
#include "stat/FollowTargetByVelocityEffect.h"
#include "stat/FragmentBurstStatEffect.h"
#include "stat/PositionStatEffect.h"
#include "stat/VelocityStatEffect.h"
#include "CommonTasks.h"
#include "curve/CurveSolver.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"

namespace StatEffect {
  template<class Tag = DefaultCurveTag>
  CurveAlias getCurveAlias() {
    return {
      QueryAlias<Row<float>>::create<CurveInput<Tag>>(),
      QueryAlias<Row<float>>::create<CurveOutput<Tag>>(),
      QueryAlias<Row<CurveDefinition*>>::create<CurveDef<Tag>>()
    };
  }

  void configureTables(IAppBuilder& builder) {
    auto temp = builder.createTask();
    temp.query<ConfigRow, FollowTargetByPositionStatEffect::CommandRow>().forEachRow([](ConfigRow& config, auto&&) {
      //Hack to account for lambda processing being the only table that is scheduled before the lifetime update,
      //so it needs to be removed one tick earlier so that it's only called once instead of twice
      config.at().curves.push_back(getCurveAlias<>());
    });

    temp.discard();
  }

  void init(IAppBuilder& builder) {
    ConstraintStatEffect::initStat(builder);
  }

  void createTasks(IAppBuilder& builder) {
    configureTables(builder);

    auto temp = builder.createTask();
    temp.discard();

    for(auto table : builder.queryTables<StatEffect::Lifetime>()) {
      StatEffect::tickLifetime(&builder, table, 0);
    }

    for(auto table : builder.queryTables<StatEffect::StatEffectTagRow>()) {
      const Config config = *temp.query<StatEffect::ConfigRow>(table).tryGetSingletonElement();
      for(const CurveAlias& curve : config.curves) {
        StatEffect::solveCurves(builder, table, curve);
      }
    }

    PositionStatEffect::processStat(builder);
    FollowTargetByPositionStatEffect::processStat(builder);
    DamageStatEffect::processStat(builder);
    VelocityStatEffect::processStat(builder);
    AreaForceStatEffect::processStat(builder);
    FollowTargetByVelocityStatEffect::processStat(builder);
    FragmentBurstStatEffect::processStat(builder);
    ConstraintStatEffect::processStat(builder);
  }

  namespace StatStorage {
    using DB = Database<
      PositionStatEffectTable,
      VelocityStatEffectTable,
      AreaForceStatEffectTable,
      FollowTargetByPositionStatEffectTable,
      FollowTargetByVelocityStatEffectTable,
      FragmentBurstStatEffectTable,
      DamageStatEffectTable,
      ConstraintStatEffectTable
    >;
  };

  void createDatabase(RuntimeDatabaseArgs& args) {
    DBReflect::addDatabase<StatStorage::DB>(args);
  }
}