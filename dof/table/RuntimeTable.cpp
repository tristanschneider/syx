#include "Precompile.h"
#include "RuntimeTable.h"
#include "IRow.h"
#include <cassert>
#include "StableElementID.h"

namespace {
  //Swap the final element into the element at index `i`, ignoring what is at that index
  void swapAndPopIntoEmpty(size_t i, StableIDRow& stable, StableElementMappings& mappings, const TableID& tableID) {
    //If it's the last element, no swap is needed, only pop
    if(stable.size() == i + 1) {
      stable.popBack();
    }
    else {
      stable.swapRemove(i, i + 1, stable.size());
      mappings.updateKey(stable.at(i).getMapping(), tableID.remakeElement(i));
    }
  }
}

RuntimeTable::RuntimeTable(RuntimeTableArgs&& args)
  : mappings{ args.mappings }
  , tableID{ args.tableID }
  , tableType{ args.rows.tableType }
{
  rows.reserve(args.rows.rows.size());
  for(const auto& row : args.rows.rows) {
    IRow*& newRow = rows[row.type];
    assert(newRow == nullptr);
    newRow = row.row;
  }
}

size_t RuntimeTable::migrate(size_t i, RuntimeTable& from, RuntimeTable& to, size_t count) {
  const UnpackedDatabaseElementID fromID = from.tableID.remakeElement(i);
  if(from.getID() == to.getID()) {
    assert(false && "Moving an element in place was likely unintentional");
    return i;
  }

  const size_t fromSize = from.size();
  const size_t dstBegin = to.size();
  const size_t dstEnd = dstBegin + count;

  //Move all common rows to the destination
  for(auto& pair : to.rows) {
    IRow* toRow = pair.second;
    IRow* fromRow = from.tryGet(pair.first);
    //This handles the case where the source row is empty and in that case adds an empty destination element
    toRow->resize(dstBegin, dstEnd);
    toRow->migrateElements(MigrateArgs{
      .fromIndex = i,
      .fromRow = fromRow,
      .count = count,
      .toIndex = dstBegin
    });
  }

  //Swap Remove from source. Could be faster to combine this with the above step while visiting,
  //but is more confusing when accounting for cases where src has rows dst doesn't
  //Skip stable row because that was already addressed in the migrate above
  for(auto& pair : from.rows) {
    if(pair.first == DBTypeID::get<StableIDRow>()) {
      StableIDRow* stable = static_cast<StableIDRow*>(pair.second);
      assert(from.mappings);
      for(size_t c = 0; c < count; ++c) {
        //The updates in the loop below will assign to the new destination.
        //This update is for the swapped element
        swapAndPopIntoEmpty(i + count - c - 1, *stable, *from.mappings, from.getID());
      }
    }
    else {
      pair.second->swapRemove(i, i + count, fromSize);
    }
  }

  assert((from.mappings != nullptr) == (to.mappings != nullptr) && "Moves are not allowed to create or destroy stable mappings");
  if(StableIDRow* stable = to.tryGet<StableIDRow>(); to.mappings && stable) {
    for(size_t r = 0; r < count; ++r) {
      const UnpackedDatabaseElementID newID = to.getID().remakeElement(dstBegin + r);
      to.mappings->updateKey(stable->at(dstBegin + r).getMapping(), newID);
    }
  }

  from.tableSize -= count;
  to.tableSize += count;
  return dstBegin;
}

void RuntimeTable::resize(size_t newSize, const ElementRef* reservedKeys) {
  for(auto& pair : rows) {
    if(pair.first == DBTypeID::get<StableIDRow>()) {
      assert(mappings);
      StableIDRow* stable = static_cast<StableIDRow*>(pair.second);
      size_t oldSize = stable->size();
      //Remove mappings for elements about to be removed
      for(size_t i = newSize; i < oldSize; ++i) {
        if (const StableElementMappingPtr m = stable->at(i).getMapping()) {
          [[maybe_unused]] const bool erased = mappings->tryEraseKey(mappings->getStableID(*m));
          assert(erased);
        }
      }

      stable->resize(tableSize, newSize);

      //Create new mappings for new elements
      for(size_t i = oldSize; i < newSize; ++i) {
        const UnpackedDatabaseElementID newId = getID().remakeElement(i);
        const ElementRef& ref = reservedKeys ? reservedKeys[i - oldSize] : ElementRef{ mappings->createKey() };
        assert(ref.getMapping());
        //Assign new id
        stable->at(i) = ref;
        //Add mapping for new id
        mappings->insertKey(ref.getMapping(), newId);
      }
    }
    else {
      pair.second->resize(tableSize, newSize);
    }
  }

  tableSize = newSize;
}

size_t RuntimeTable::addElements(size_t count, const ElementRef* reservedKeys) {
  size_t result = size();
  resize(result + count, reservedKeys);
  return result;
}

size_t RuntimeTable::size() const {
  return tableSize;
}

size_t RuntimeTable::rowCount() const {
  return rows.size();
}

void RuntimeTable::swapRemove(size_t i) {
  //Swap remove all rows, erase and update stable ids
  for(auto& pair : rows) {
    if(pair.first == DBTypeID::get<StableIDRow>()) {
      assert(mappings);
      StableIDRow* stable = static_cast<StableIDRow*>(pair.second);

      mappings->eraseKey(stable->at(i).getMapping());

      swapAndPopIntoEmpty(i, *stable, *mappings, getID());
    }
    else {
      pair.second->swapRemove(i, i + 1, tableSize);
    }
  }
  --tableSize;
}
