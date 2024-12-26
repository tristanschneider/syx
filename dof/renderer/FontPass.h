#pragma once

#include "Table.h"

class IAppBuilder;

struct FONScontext;

namespace FontPass {
  using FontHandle = int;

  struct Globals {
    FONScontext* fontContext{};
    int defaultFont{};
  };
  struct GlobalsRow : SharedRow<Globals> {};

  void init(IAppBuilder& builder);
  void render(IAppBuilder& builder);
}