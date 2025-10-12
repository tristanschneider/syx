#pragma once

#include <vector>

namespace gnx::Container {
  template<class T>
  void swapRemove(T& container, size_t index) {
    container[index] = std::move(container.back());
    container.pop_back();
  }

  //Make a vector from the parameters which supports moving uncopyable arguments, unlike the initializer list constructor.
  template<class T, class... Rest>
  std::vector<T> makeVector(T&& t, Rest&&... rest) {
    std::array<T, sizeof...(rest) + 1> temp{ std::forward<T>(t), std::forward<Rest>(rest)... };
    std::vector<T> result;
    result.reserve(temp.size());
    result.insert(result.end(), std::make_move_iterator(temp.begin()), std::make_move_iterator(temp.end()));
    return result;
  }
}