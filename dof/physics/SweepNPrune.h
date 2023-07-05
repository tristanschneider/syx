#pragma once

#include "glm/vec2.hpp"
#include "Scheduler.h"

namespace Broadphase {
  //The key provided by the user to identify this object in collision pairs
  using UserKey = size_t;

  //The key provided by the broadphase to point at the bounds storage
  struct BroadphaseKey {
    bool operator==(const BroadphaseKey& rhs) const {
      return value == rhs.value;
    }
    bool operator!=(const BroadphaseKey& rhs) const {
      return !(*this == rhs);
    }
    size_t value{};
  };

  struct SweepCollisionPair {
    SweepCollisionPair() = default;
    SweepCollisionPair(BroadphaseKey ka, BroadphaseKey kb)
      : a{ ka }
      , b{ kb } {
      if(a.value > b.value) {
        std::swap(a, b);
      }
    }

    bool operator==(const SweepCollisionPair& r) const {
      return a.value == r.a.value && b.value == r.b.value;
    }

    bool operator<(const SweepCollisionPair& r) const {
      return a.value == r.b.value ? b.value < r.b.value : a.value < r.a.value;
    }

    BroadphaseKey a{};
    BroadphaseKey b{};
  };

  struct SwapLog {
    std::vector<SweepCollisionPair>& gains;
    std::vector<SweepCollisionPair>& losses;
  };

  struct SweepElement {
    static constexpr size_t START_BIT = size_t(1) << (sizeof(size_t)*8 - 1);

    static SweepElement createBegin(BroadphaseKey v) {
      return { v.value | START_BIT };
    }

    static SweepElement createEnd(BroadphaseKey v) {
      return { v.value };
    }

    bool isStart() const {
      return value.value & START_BIT;
    }

    bool isEnd() const {
      return !isStart();
    }

    size_t getValue() const {
      return value.value & (~START_BIT);
    }

    BroadphaseKey value{};
  };

  struct SweepAxis {
    //List of indices into the sweep2d values, sorted by this axis bounds
    //Free list is used to guarantee index stability
    std::vector<SweepElement> elements;
  };

  struct Sweep2D {
    static constexpr size_t S = 2;
    static constexpr float REMOVED = std::numeric_limits<float>::max();
    static constexpr float NEW = std::numeric_limits<float>::min();

    std::array<SweepAxis, S> axis;
    std::array<std::vector<std::pair<float, float>>, S> bounds;
    std::vector<UserKey> userKey;
    //Keys ready for re-use
    std::vector<BroadphaseKey> freeList;
    //Keys recently marked for removal but won't be moved to the free list until the next recomputePairs
    std::vector<BroadphaseKey> pendingRemoval;
  };

  namespace SweepNPrune {
    //Insert the given user keys and mapping them to out keys
    //Object will be sorted into place during next recomputePairs
    void insertRange(Sweep2D& sweep,
      const UserKey* userKeys,
      BroadphaseKey* outKeys,
      size_t count);

    void eraseRange(Sweep2D& sweep,
      const BroadphaseKey* keys,
      size_t count);

    void updateBoundaries(Sweep2D& sweep,
      const float* minX,
      const float* maxX,
      const float* minY,
      const float* maxY,
      const BroadphaseKey* keys,
      size_t count);

    void recomputePairs(Sweep2D& sweep, SwapLog& log);
  };

  //TODO: dealing with keys is complicated
  //Could think of it as max of 4 keys
  //Could share all in one db, this is probably easiest
  namespace SweepGrid {
    static constexpr size_t EMPTY = std::numeric_limits<size_t>::max();
    static constexpr BroadphaseKey EMPTY_KEY{ EMPTY };

    struct GridDefinition {
      glm::vec2 bottomLeft{};
      glm::vec2 cellSize{};
      size_t cellsX{};
      size_t cellsY{};
    };
    //Keys corresponding to an element in a given cell
    struct CellKey {
      //The key of the cell
      BroadphaseKey cellKey{ EMPTY };
      //The key of the element in the cell
      BroadphaseKey elementKey{ EMPTY };
    };
    struct KeyMapping {
      static constexpr size_t S = 4;
      std::array<CellKey, S> publicToPrivate;
    };
    struct GridMappings {
      std::vector<KeyMapping> mappings;
      std::vector<UserKey> userKeys;
    };
    struct Grid {
      GridDefinition definition;
      GridMappings mappings;
      std::vector<Sweep2D> cells;
      std::vector<BroadphaseKey> freeList;
    };

    void init(Grid& grid);

    void insertRange(Grid& sweep,
      const UserKey* userKeys,
      BroadphaseKey* outKeys,
      size_t count);

    void eraseRange(Grid& sweep,
      const BroadphaseKey* keys,
      size_t count);

    void updateBoundaries(Grid& sweep,
      const float* minX,
      const float* maxX,
      const float* minY,
      const float* maxY,
      const BroadphaseKey* keys,
      size_t count);

    TaskRange recomputePairs(Grid& grid, SwapLog finalResults);
    void recomputePairs(Grid& sweep, SwapLog& log);
  }
}


template<>
struct std::hash<Broadphase::SweepCollisionPair> {
  std::size_t operator()(const Broadphase::SweepCollisionPair& s) const noexcept {
    std::hash<size_t> h;
    //cppreference hash combine example
    return h(s.a.value) ^ (h(s.b.value) << 1);
  }
};