#pragma once

namespace gnx::Container {
  template<class T>
  void swapRemove(T& container, size_t index) {
    container[index] = std::move(container.back());
    container.pop_back();
  }
}