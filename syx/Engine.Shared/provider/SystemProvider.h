#pragma once
#include "util/TypeId.h"

class System;

class SystemProvider {
public:
  template<typename T>
  T* getSystem() {
    static_assert(std::is_base_of_v<System, T>, "getSystem should only be used for systems");
    return static_cast<T*>(_getSystem(typeId<T, System>()));
  }

  virtual System* _getSystem(size_t id) = 0;
};