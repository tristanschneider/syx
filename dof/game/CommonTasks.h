#pragma once

#include "Scheduler.h"
#include "Table.h"

namespace CommonTasks {
  //Namespace for doing an operation "Now" vs returning a task
  namespace Now {
    //Move or opy all elements from src into dst starting at dst[dstStart]. Caller is responsible for ensuring dst has required space
    template<class T>
    void moveOrCopyRow(const Row<T>& src, Row<T>& dst, size_t dstStart) {
      if constexpr(std::is_trivially_copyable_v<T>) {
        if(size_t size = src.size()) {
          std::memcpy(&dst.at(dstStart), &src.at(0), sizeof(T)*size);
        }
      }
      else {
        for(size_t i = 0; i < src.size(); ++i) {
          dst.at(dstStart + i) = std::move(src.at(i));
        }
      }
    }

    template<class T>
    void copyRow(const Row<T>& src, Row<T>& dst, size_t dstStart) {
      if constexpr(std::is_trivially_copyable_v<T>) {
        if(size_t size = src.size()) {
          std::memcpy(&dst.at(dstStart), &src.at(0), sizeof(T)*size);
        }
      }
      else {
        for(size_t i = 0; i < src.size(); ++i) {
          dst.at(dstStart + i) = src.at(i);
        }
      }
    }
  }

  //Move or copy all elements from src to dst starting at dst[0] and assuming dst has enough space
  template<class T>
  std::shared_ptr<TaskNode> moveOrCopyRowSameSize(const Row<T>& src, Row<T>& dst) {
    return TaskNode::create([&src, &dst](...) {
      Now::moveOrCopyRow(src, dst, 0);
    });
  }

  template<class SrcRow, class DstRow, class TableT>
  std::shared_ptr<TaskNode> moveOrCopyRowSameSize(TableT& table) {
    return moveOrCopyRowSameSize(std::get<SrcRow>(table.mRows), std::get<DstRow>(table.mRows));
  }

  template<class T>
  std::shared_ptr<TaskNode> copyRowSameSize(const Row<T>& src, Row<T>& dst) {
    return TaskNode::create([&src, &dst](...) {
      Now::copyRow(src, dst, 0);
    });
  }

  template<class SrcRow, class DstRow, class TableT>
  std::shared_ptr<TaskNode> copyRowSameSize(TableT& table) {
    return moveOrCopyRowSameSize(std::get<SrcRow>(table.mRows), std::get<DstRow>(table.mRows));
  }
}