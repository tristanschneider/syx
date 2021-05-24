#pragma once
#include "SyxSIMD.h"
#define DISABLE_WARNING_START(x) \
__pragma(warning(push)) \
__pragma(warning(disable : x)) \

#define DISABLE_WARNING_END __pragma(warning(pop))
DISABLE_WARNING_START(4100)

namespace Syx {
  //http://stackoverflow.com/questions/8456236/how-is-a-vectors-data-aligned
  template <typename T, size_t N = SAlignment>
  class AlignmentAllocator {
  public:
    typedef T value_type;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    typedef T * pointer;
    typedef const T* const_pointer;

    typedef T& reference;
    typedef const T& const_reference;

  public:
    inline AlignmentAllocator() throw () {}

    template <typename T2>
    inline AlignmentAllocator(const AlignmentAllocator<T2, N>&) throw () {}

    inline ~AlignmentAllocator() throw () {}

    inline pointer adress(reference r) {
      return &r;
    }

    inline const_pointer adress(const_reference r) const {
      return &r;
    }

    inline pointer allocate(size_type n) {
      return (pointer)_aligned_malloc(n*sizeof(value_type), N);
    }

    inline void deallocate(pointer p, size_type) {
      _aligned_free(p);
    }

    inline size_type max_size() const throw () {
      return size_type(-1) / sizeof(value_type);
    }

    template <typename T2>
    struct rebind {
      typedef AlignmentAllocator<T2, N> other;
    };

    bool operator!=(const AlignmentAllocator<T, N>& other) const {
      return !(*this == other);
    }

    // Returns true if and only if storage allocated from *this
    // can be deallocated from other, and vice versa.
    // Always returns true for stateless allocators.
    bool operator==(const AlignmentAllocator<T, N>& /*other*/) const {
      return true;
    }
  };
}

#define SVec3Vec std::vector<SFloats, AlignmentAllocator<SFloats>>
#define Vec3Vec std::vector<Vec3, AlignmentAllocator<Vec3>>

DISABLE_WARNING_END