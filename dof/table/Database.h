#pragma once

template<class... Tables>
struct Database {
  //Call a single argument visitor for each row
  template<class Visitor, class... Args>
  constexpr void visitOne(const Visitor& visitor, Args&&... args) {
    (visitor(std::get<Tables>(mTables), args...), ...);
  }

  //Call a multi-argument visitor with all rows
  template<class Visitor, class... Args>
  constexpr auto visitAll(const Visitor& visitor, Args&&... args) {
    return visitor(args..., std::get<Tables>(mTables)...);
  }

  std::tuple<Tables...> mTables;
};