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
  size_t fromSize{};
  size_t count{};
  size_t toSize{};
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

  //Move an element at `from` index to the back of this, which the caller ensures is the same row type as `fromRow`
  //Returns the index of the new element in this
  //`fromRow` can also be null, which then means just add one
  //size of fromRow is unchanged
  virtual size_t migrateElements(const MigrateArgs& args) = 0;
};