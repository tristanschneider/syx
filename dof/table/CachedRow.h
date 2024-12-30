#pragma once

//Intended for use when iterating over many ids, this reuses the fetched table in the common case,
//then swaps it out if the id is from a different table. The assumption is that large groups of
//ids are all from the same table
template<class T>
struct CachedRow {
  explicit operator bool() const { return row; }
  const T* operator->() const { return row; }
  T* operator->() { return row; }
  const T& operator*() const { return *row; }
  T& operator*() { return *row; }

  T* row{};
  size_t tableID{};
};
