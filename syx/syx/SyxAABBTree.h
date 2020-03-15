#pragma once
#include "SyxBroadphase.h"

namespace Syx {
  namespace Create {
    std::unique_ptr<Broadphase> aabbTree();
  }
}
