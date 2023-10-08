#pragma once

#include "stat/StatEffectBase.h"

class IAppBuilder;
class RuntimeDatabase;

namespace LambdaStatEffect {
  struct Args {
    RuntimeDatabase* db{};
    StableElementID resolvedID;
  };
  using Lambda = std::function<void(Args&)>;
  struct LambdaRow : Row<Lambda> {};

  void processStat(IAppBuilder& builder);
};

struct LambdaStatEffectTable : StatEffectBase<
  LambdaStatEffect::LambdaRow
> {};
