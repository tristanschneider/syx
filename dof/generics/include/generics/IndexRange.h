#pragma once

#include <iterator>

namespace gnx {
  template<class T>
  class IndexRangeT {
  public:
    class Iterator {
    public:
      using Pointer = std::add_pointer_t<T>;
      using Reference = std::add_lvalue_reference_t<T>;

      using iterator_category = std::bidirectional_iterator_tag;
      using value_type        = T;
      using difference_type   = size_t;
      using pointer           = Pointer;
      using reference         = Reference;

      Iterator& operator=(const Iterator&) = default;

      Pointer operator->() { return &value; }
      const Pointer operator->() const { return &value; }
      const Reference operator*() const { return value; }
      Reference operator*() { return value; }
      auto operator<=>(const Iterator&) const = default;

      Iterator& operator++() {
        ++value;
        return *this;
      }

      Iterator operator++(int) {
        auto result{ *this };
        ++(*this);
        return result;
      }

      Iterator& operator--() {
        --value;
        return *this;
      }

      Iterator operator--(int) {
        auto result{ *this };
        --(*this);
        return result;
      }

      T value{};
    };

    IndexRangeT() = default;
    IndexRangeT(T b, T e)
      : beginIndex{ b }
      , endIndex{ e }
    {};
    IndexRangeT(const IndexRangeT&) = default;

    IndexRangeT& operator=(const IndexRangeT&) = default;
    auto operator<=>(const IndexRangeT&) const = default;

    Iterator begin() const {
      return { beginIndex };
    }

    Iterator end() const {
      return { endIndex };
    }

  private:
    T beginIndex{};
    T endIndex{};
  };

  template<class T>
  auto makeIndexRange(T begin, T end) {
    return IndexRangeT<T>{ begin, end };
  }

  template<class T>
  auto makeIndexRangeBeginCount(T begin, T count) {
    return makeIndexRange(begin, begin + count);
  }

  using IndexRange = IndexRangeT<size_t>;
}