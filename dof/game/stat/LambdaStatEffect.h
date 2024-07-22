#pragma once

#include "stat/StatEffectBase.h"

class IAppBuilder;
class RuntimeDatabase;

namespace LambdaStatEffect {
  struct Args {
    RuntimeDatabase* db{};
    ElementRef resolvedID;
  };
  using Lambda = std::function<void(Args&)>;
  struct LambdaRow : Row<Lambda> {};

  void processStat(IAppBuilder& builder);

  class Builder : public StatEffect::BuilderBase {
  public:
    Builder(AppTaskArgs& args);

    Builder& setLambda(const Lambda& l);

  private:
    LambdaRow* command{};
  };
};

struct LambdaStatEffectTable : StatEffectBase<
  LambdaStatEffect::LambdaRow
> {};
