#pragma once

#include "glm/vec2.hpp"
#include "Scheduler.h"

class IAppBuilder;

namespace Broadphase {
  //The key provided by the user to identify this object in collision pairs
  using UserKey = size_t;
  struct SweepCollisionPair {
    SweepCollisionPair() = default;
    SweepCollisionPair(UserKey ka, UserKey kb)
      : a{ ka }
      , b{ kb } {
      if(a > b) {
        std::swap(a, b);
      }
    }

    bool operator==(const SweepCollisionPair& r) const {
      return a == r.a && b == r.b;
    }

    bool operator<(const SweepCollisionPair& r) const {
      return a == r.a ? b < r.b : a < r.a;
    }

    UserKey a{};
    UserKey b{};
  };
  //The key provided by the broadphase to point at the bounds storage
  struct BroadphaseKey {
    bool operator==(const BroadphaseKey& rhs) const {
      return value == rhs.value;
    }
    bool operator!=(const BroadphaseKey& rhs) const {
      return !(*this == rhs);
    }
    bool operator<(const BroadphaseKey& rhs) const {
      return value < rhs.value;
    }
    size_t value{};
  };
}

template<>
struct std::hash<Broadphase::SweepCollisionPair> {
  std::size_t operator()(const Broadphase::SweepCollisionPair& s) const noexcept {
    std::hash<size_t> h;
    //cppreference hash combine example
    return h(s.a) ^ (h(s.b) << 1);
  }
};
template<>
struct std::hash<Broadphase::BroadphaseKey> {
  std::size_t operator()(const Broadphase::BroadphaseKey& s) const noexcept {
    return std::hash<size_t>()(s.value);
  }
};

namespace Broadphase {
  //This holds the user keys and boundaries of the shapes tracked by the broadphase
  //The spatial data structure references this to decide how to partition the objects
  struct ObjectDB {
    static constexpr size_t S = 2;
    static constexpr float NEW = std::numeric_limits<float>::min();
    static constexpr float REMOVED = std::numeric_limits<float>::max();
    static constexpr size_t EMPTY = std::numeric_limits<size_t>::max();
    static constexpr BroadphaseKey EMPTY_KEY{ EMPTY };
    using BoundsMinMax = std::pair<float, float>;
    using BoundsAxis = std::vector<std::pair<float, float>>;

    std::array<BoundsAxis, S> bounds;
    std::vector<UserKey> userKey;
    //Keys ready for re-use
    std::vector<BroadphaseKey> freeList;
    //Keys recently marked for removal but won't be moved to the free list until the next recomputePairs
    std::vector<BroadphaseKey> pendingRemoval;
  };
  struct PairTracker {
    //This uses SweepCollisionPair for convenience but is actually a pair of BroadphaseKey not UserKey
    std::unordered_set<SweepCollisionPair> trackedPairs;
  };

  struct SwapLog {
    //These contain BroadphaseKeys rather than UserKeys until the final step of recomputePairs that updates PairTracker
    std::vector<SweepCollisionPair>& gains;
    std::vector<SweepCollisionPair>& losses;
  };
  struct ConstSwapLog {
    const std::vector<SweepCollisionPair>& gains;
    const std::vector<SweepCollisionPair>& losses;
  };

  //Generates new keys and adds the obects to the db. Should be used in combination with insertion into
  //the spatial strucures that reference this
  void insertRange(ObjectDB& db,
    const UserKey* userKeys,
    BroadphaseKey* outKeys,
    size_t count);
  //Marks elements for deletion by putting them in the pending removal list and setting the bounds to REMOVED
  //The next update of the dependent spatial structures should notice this to cause their removal
  void eraseRange(ObjectDB& db,
    const BroadphaseKey* keys,
    size_t count);
  //Writes the new boundaries. Dependant spatial structures should notice this in their next update
  void updateBoundaries(ObjectDB& db,
    const float* minX,
    const float* maxX,
    const float* minY,
    const float* maxY,
    const BroadphaseKey* keys,
    size_t count
  );
  //Log events for pairs that will be removed as a result of pending removals, but don't remove yet
  void logPendingRemovals(const ObjectDB& db, SwapLog& log, const PairTracker& pairs);
  //Take the results of recomputePairs, resolve duplicates, and transform the keys from BroadphaseKey to UserKey
  void logChangedPairs(const ObjectDB& db, PairTracker& pairs, const ConstSwapLog& changedPairs, SwapLog& output);
  //Remove elements pending deletion. This is after the events for them have already been logged and no cells are referencing them anymore
  void processPendingRemovals(ObjectDB& db);

  struct SweepElement {
    static constexpr size_t END_BIT = size_t(1) << (sizeof(size_t)*8 - 1);

    bool operator==(const SweepElement& rhs) const {
      return value == rhs.value;
    }

    bool operator<(const SweepElement& rhs) const {
      return value < rhs.value;
    }

    //Put the bit on the end so that end always comes after start in sort order of the raw value
    static SweepElement createEnd(BroadphaseKey v) {
      return { v.value | END_BIT };
    }

    static SweepElement createBegin(BroadphaseKey v) {
      return { v.value };
    }

    bool isEnd() const {
      return value.value & END_BIT;
    }

    bool isStart() const {
      return !isEnd();
    }

    size_t getValue() const {
      return value.value & (~END_BIT);
    }

    BroadphaseKey value{};
  };

  struct SweepAxis {
    //List of indices into the sweep2d values, sorted by this axis bounds
    //Free list is used to guarantee index stability
    std::vector<SweepElement> elements;
    //If elements are found outside of this range the shapes are removed from this Sweep2D
    //Default to max float extents meaning unlimited
    float min = std::numeric_limits<float>::lowest();
    float max = std::numeric_limits<float>::max();
  };

  struct Sweep2D {
    static constexpr size_t S = 2;
    std::array<SweepAxis, S> axis;
    std::unordered_set<BroadphaseKey> containedKeys;
    std::vector<BroadphaseKey> temp;
  };

  namespace Debug {
    bool isValidSweepAxis(const SweepAxis& axis);
  }

  namespace SweepNPrune {
    //Insert the given user keys and mapping them to out keys
    //Object will be sorted into place during next recomputePairs
    void insertRange(Sweep2D& sweep,
      const BroadphaseKey* keys,
      size_t count
    );
    //Inserts if it wasn't already there
    void tryInsertRange(Sweep2D& sweep,
      const BroadphaseKey* keys,
      size_t count
    );
    //Erase isn't needed directly, instead the bounds are marked as outside the cell which will cause removal as part of recomputePairs

    void recomputeCandidates(Sweep2D& sweep, const ObjectDB& db, const PairTracker& pairs, SwapLog& log);
    void recomputePairs(Sweep2D& sweep, const ObjectDB& db, const PairTracker& pairs, SwapLog& log);
  };

  namespace SweepGrid {
    static constexpr size_t EMPTY = std::numeric_limits<size_t>::max();
    static constexpr BroadphaseKey EMPTY_KEY{ EMPTY };

    struct GridDefinition {
      glm::vec2 bottomLeft{};
      glm::vec2 cellSize{};
      size_t cellsX{};
      size_t cellsY{};
    };
    struct Grid {
      ObjectDB objects;
      PairTracker pairs;
      GridDefinition definition;
      std::vector<Sweep2D> cells;
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

    void recomputePairs(IAppBuilder& builder);
  }
}