#pragma once

#include "AppBuilder.h"
#include "Scheduler.h"
#include "Table.h"
#include "Profile.h"

namespace CommonTasks {
  //Namespace for doing an operation "Now" vs returning a task
  namespace Now {
    //Move or opy all elements from src into dst starting at dst[dstStart]. Caller is responsible for ensuring dst has required space
    template<class T>
    void moveOrCopyRow(const Row<T>& src, Row<T>& dst, size_t dstStart) {
      PROFILE_SCOPE("Common", "CopyRow");
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

    //Allow different types if they are plain types of the same size
    template<class A, class B>
    void moveOrCopyRow(const Row<A>& src, Row<B>& dst, size_t dstStart) {
      PROFILE_SCOPE("Common", "CopyRow");
      if constexpr(std::is_trivially_copyable_v<A> &&
        std::is_trivially_copyable_v<B> &&
        sizeof(A) == sizeof(B)) {
        if(size_t size = src.size()) {
          std::memcpy(&dst.at(dstStart), &src.at(0), sizeof(A)*size);
        }
      }
      else {
        static_assert(sizeof(A) == -1, "Must be same size and trivially copyable");
      }
    }

    //Shared row is the same but only copy the single element that's there rather than the table size
    template<class T>
    void moveOrCopyRow(const SharedRow<T>& src, SharedRow<T>& dst, size_t) {
      if constexpr(std::is_trivially_copyable_v<T>) {
        std::memcpy(&dst.at(0), &src.at(0), sizeof(T));
      }
      else {
        dst.at(0) = std::move(src.at(0));
      }
    }

    template<class A, class B>
    void moveOrCopyRow(const SharedRow<A>& src, SharedRow<B>& dst, size_t) {
      if constexpr(std::is_trivially_copyable_v<A> &&
        std::is_trivially_copyable_v<B> &&
        sizeof(A) == sizeof(B)) {
        std::memcpy(&dst.at(0), &src.at(0), sizeof(A));
      }
      else {
        static_assert(sizeof(A) == -1, "Must be same size and trivially copyable");
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

  template<class SrcRow, class DstRow>
  void moveOrCopyRowSameSize(IAppBuilder& builder, const UnpackedDatabaseElementID& srcTable, const UnpackedDatabaseElementID& dstTable) {
    auto task = builder.createTask();
    task.setName(FUNC_NAME);
    auto src = task.query<const SrcRow>(srcTable);
    auto dst = task.query<DstRow>(dstTable);
    assert(src.size() && dst.size());
    task.setCallback([src, dst](AppTaskArgs&) mutable {
      CommonTasks::Now::moveOrCopyRow(src.get<0>(0), dst.get<0>(0), 0);
    });
    builder.submitTask(std::move(task));
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