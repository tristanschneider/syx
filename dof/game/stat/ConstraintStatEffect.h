#pragma once

#include "Constraints.h"
#include "stat/StatEffectBase.h"
#include "glm/vec2.hpp"

namespace ConstraintStatEffect {
  struct TargetA : Constraints::ExternalTargetRow {};
  struct TargetB : Constraints::ExternalTargetRow {};
  struct ConstraintCommon : Constraints::ConstraintCommonRow {};
  struct ConstraintA : Constraints::ConstraintSideRow {};
  struct ConstraintB : Constraints::ConstraintSideRow {};
  struct JointRow : Constraints::JointRow {};
  struct StorageRow : Constraints::ConstraintStorageRow {};
  struct TickTracker {
    size_t ticksSinceSweep{};
  };
  struct TickTrackerRow : SharedRow<TickTracker> {};

  //Uses global list to inform physics system of creation and removal
  //Physics system handles the actual constraint logic through the existence of the tables
  void processStat(IAppBuilder& builder);
  void initStat(IAppBuilder& builder);

  class Builder : public StatEffect::BuilderBase {
  public:
    Builder(AppTaskArgs& args);

    Constraints::Builder& constraintBuilder();

  private:
    Constraints::Builder builder;
  };
}

struct ConstraintStatEffectTable : StatEffectBase<
  Constraints::TableConstraintDefinitionsRow,
  Constraints::ConstraintChangesRow,
  ConstraintStatEffect::TargetA,
  ConstraintStatEffect::TargetB,
  ConstraintStatEffect::ConstraintCommon,
  ConstraintStatEffect::ConstraintA,
  ConstraintStatEffect::ConstraintB,
  ConstraintStatEffect::JointRow,
  ConstraintStatEffect::StorageRow,
  ConstraintStatEffect::TickTrackerRow
> {};