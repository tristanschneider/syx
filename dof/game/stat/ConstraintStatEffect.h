#pragma once

#include "Constraints.h"
#include "stat/StatEffectBase.h"
#include "glm/vec2.hpp"
#include "generics/RateLimiter.h"

#include "SweepNPruneBroadphase.h"

namespace ConstraintStatEffect {
  struct TargetA : Constraints::ExternalTargetRow {};
  struct TargetB : Constraints::ExternalTargetRow {};
  struct CustomConstraint : Constraints::CustomConstraintRow {};
  struct JointRow : Constraints::JointRow {};
  struct StorageRow : Constraints::ConstraintStorageRow {};
  struct TickTracker {
    gnx::OneInHundredRateLimit rateLimiter;
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
  Constraints::AutoManageJointTag,
  Constraints::TableConstraintDefinitionsRow,
  Constraints::ConstraintChangesRow,
  SweepNPruneBroadphase::BroadphaseKeys,
  ConstraintStatEffect::TargetA,
  ConstraintStatEffect::TargetB,
  ConstraintStatEffect::CustomConstraint,
  ConstraintStatEffect::JointRow,
  ConstraintStatEffect::StorageRow,
  ConstraintStatEffect::TickTrackerRow
> {};