#include "Precompile.h"
#include "RuntimeTable.h"
#include "IRow.h"
#include <cassert>
#include "StableElementID.h"

RuntimeTable::RuntimeTable(RuntimeTableArgs&& args)
  : mappings{ args.mappings }
  , tableID{ args.tableID }
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
  //Move all common rows to the destination
  size_t result{};
  for(auto& pair : to.rows) {
    IRow* toRow = pair.second;
    IRow* fromRow = from.tryGet(pair.first);
    //This handles the case where the source row is empty and in that case adds an empty destination element
    result = toRow->migrateElements(i, fromRow, count);
  }

  //Swap Remove from source. Could be faster to combine this with the above step while visiting,
  //but is more confusing when accounting for cases where src has rows dst doesn't
  //Skip stable row because that was already addressed in the migrate above
  for(auto& pair : from.rows) {
    for(size_t c = 0; c < count; ++c) {
      pair.second->swapRemove(i + c);
    }
  }

  assert((from.mappings != nullptr) == (to.mappings != nullptr) && "Moves are not allowed to create or destroy stable mappings");
  if(StableIDRow* stable = to.tryGet<StableIDRow>(); to.mappings && stable) {
    for(size_t r = 0; r < count; ++r) {
      const UnpackedDatabaseElementID newID = to.getID().remakeElement(result + r);
      to.mappings->updateKey(stable->at(result + r).getMapping(), newID.mValue);
    }
  }

  from.tableSize -= count;
  to.tableSize += count;
  return result;
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

      stable->resize(newSize);

      //Create new mappings for new elements
      for(size_t i = oldSize; i < newSize; ++i) {
        const UnpackedDatabaseElementID newId = getID().remakeElement(i);
        const ElementRef& ref = reservedKeys ? reservedKeys[i - oldSize] : ElementRef{ mappings->createKey() };
        assert(ref.getMapping());
        //Assign new id
        stable->at(i) = ref;
        //Add mapping for new id
        mappings->insertKey(ref.getMapping(), newId.mValue);
      }
    }
    else {
      pair.second->resize(newSize);
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

void RuntimeTable::swapRemove(size_t i) {
  //Swap remove all rows, erase and update stable ids
  for(auto& pair : rows) {
    if(pair.first == DBTypeID::get<StableIDRow>()) {
      assert(mappings);
      StableIDRow* stable = static_cast<StableIDRow*>(pair.second);

      mappings->eraseKey(stable->at(i).getMapping());

      if(size_t toSwap = stable->size() - 1 > i) {
        stable->at(i) = std::move(stable->at(toSwap));
        mappings->updateKey(stable->at(i).getMapping(), getID().remakeElement(i).mValue);
      }

      stable->mElements.pop_back();
    }
    else {
      pair.second->swapRemove(i);
    }
  }
  --tableSize;
}
