#pragma once

template<class... Tables>
struct Database {
  template<class T, class Visitor, size_t... I>
  static constexpr void visitImpl(T& tuple, std::index_sequence<I...>, const Visitor& visitor) {
    (visitor(std::get<I>(tuple)), ...);
  }

  //Call a single argument visitor for each row
  template<class Visitor, class... Args>
  constexpr void visitOne(const Visitor& visitor) {
    visitImpl(mTables, std::make_index_sequence<sizeof...(Tables)>(), visitor);
  }

  template<class Visitor>
  constexpr void visitOne(const Visitor& visitor) const {
    visitImpl(mTables, std::make_index_sequence<sizeof...(Tables)>(), visitor);
  }

  static constexpr size_t size() {
    return sizeof...(Tables);
  }

  std::tuple<Tables...> mTables;
};