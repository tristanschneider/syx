#pragma once
#include "util/TypeId.h"

class System;

class SystemProvider {
public:
  template<typename T>
  T* getSystem() {
    return static_cast<T*>(_getSystem(typeId<T, System>()));
  }

  virtual System* _getSystem(size_t id) = 0;
};