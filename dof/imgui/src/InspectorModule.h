#pragma once

#include "StableElementID.h"

class IAppBuilder;

namespace InspectorModule {
  struct Selection {
    ElementRef ref;
    size_t stableID{};
  };
  struct InspectorData {
    std::vector<Selection> selected;
  };
  struct InspectorRow : SharedRow<InspectorData> {};

  void update(IAppBuilder& builder);
};