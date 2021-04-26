#pragma once
#include "SyxMaterial.h"

namespace Syx {
  struct IMaterialRepository {
    virtual ~IMaterialRepository() = default;
    virtual std::unique_ptr<IMaterialHandle> addMaterial(const Material& newMaterial) = 0;
    //Returns the number of materials removed
    virtual size_t garbageCollect() = 0;
  };

  namespace Create {
    std::unique_ptr<IMaterialRepository> defaultMaterialRepository();
  }
}