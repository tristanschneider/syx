#pragma once

struct RowBuffer {
  void* elements{};
};

struct ConstRowBuffer {
  const void* elements{};
};

class IRow;

struct MigrateArgs {
  size_t fromIndex{};
  IRow* fromRow{};
  size_t count{};
  size_t toIndex{};
};

//Row of something in a table. The table knows what the type is from a DBTypeID
class IRow {
public:
  virtual ~IRow() = default;
  //Intended for cases where the caller knows the underlying type to cast to by checking the type id
  virtual RowBuffer getElements() = 0;
  virtual ConstRowBuffer getElements() const = 0;

  virtual void resize(size_t oldSize, size_t newSize) = 0;
  //Caller is responsible for validity of index
  virtual void swapRemove(size_t begin, size_t end, size_t tableSize) = 0;

  //Move elements at `fromIndex` to the specified index in this, which the caller ensures is the same row type as `fromRow`
  //The caller also ensures the size fits, as this doesn't change the size of either row
  //`fromRow` can also be null, which then means just add one
  virtual void migrateElements(const MigrateArgs& args) = 0;
};