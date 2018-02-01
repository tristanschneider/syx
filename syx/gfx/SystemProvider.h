#pragma once

class System;

class SystemProvider {
public:
  template<typename T>
  T* getSystem() {
    return static_cast<T*>(_getSystem(GetSystemID(T)));
  }

  virtual System* _getSystem(size_t id) = 0;
};