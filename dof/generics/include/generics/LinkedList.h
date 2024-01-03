#pragma once

//Templates for comon operations on intrusive linked lists
namespace gnx::LinkedList {
  template<class T>
  struct Traits {
    static size_t& getIndex(T&);
    static const size_t& getIndex(const T&);
  };

  template<class T, class IndexT, class V, class AccessT, class AccessV>
  void insertAfter(T& after, V& toInsert, IndexT insertIndex, AccessT, AccessV) {
    auto& afterI = AccessT::getIndex(after);
    auto& toInsertI = AccessV::getIndex(toInsert);
    toInsertI = afterI;
    afterI = insertIndex;
  }

  template<class T, class IndexT>
  void insertAfter(T& after, T& toInsert, IndexT insertIndex) {
    insertAfter(after, toInsert, insertIndex, Traits<T>{}, Traits<T>{});
  }

  template<class T, class IndexT, class AccessT>
  void insertAfter(T& after, T& toInsert, IndexT insertIndex, AccessT access) {
    insertAfter(after, toInsert, insertIndex, access, access);
  }

  template<class T, class V, class IndexT>
  void insertAfter(T& after, V& toInsert, IndexT insertIndex) {
    insertAfter(after, toInsert, insertIndex, Traits<T>{}, Traits<V>{});
  }

  template<class T, class V, class IndexT, class AccessT>
  void insertAfter(T& after, V& toInsert, IndexT insertIndex, AccessT accessT) {
    insertAfter(after, toInsert, insertIndex, accessT, Traits<V>{});
  }

  template<class T, class IndexT, class CallbackT, class AccessT>
  void foreach(std::vector<T>& container, IndexT begin, const CallbackT& callback, AccessT) {
    while(begin < container.size()) {
      T* node = &container[begin];
      begin = AccessT::getIndex(*node);
      if constexpr(std::is_same_v<bool, decltype(callback(*node))>) {
        if(!callback(*node)) {
          return;
        }
      }
      else {
        callback(*node);
      }
    }
  }

  template<class T, class IndexT, class CallbackT>
  void foreach(std::vector<T>& container, IndexT begin, const CallbackT& callback) {
    foreach(container, begin, callback, Traits<T>{});
  }

  template<class T, class IndexT, class CallbackT, class AccessT>
  void foreach(const std::vector<T>& container, IndexT begin, const CallbackT& callback, AccessT) {
    while(begin < container.size()) {
      const T* node = &container[begin];
      begin = AccessT::getIndex(*node);
      if constexpr(std::is_same_v<bool, decltype(callback(*node))>) {
        if(!callback(*node)) {
          return;
        }
      }
      else {
        callback(*node);
      }
    }
  }

  template<class T, class IndexT, class CallbackT>
  void foreach(const std::vector<T>& container, IndexT begin, const CallbackT& callback) {
    foreach(container, begin, callback, Traits<T>{});
  }
}