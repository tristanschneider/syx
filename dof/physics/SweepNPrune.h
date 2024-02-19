#pragma once

#include "glm/vec2.hpp"
#include "Scheduler.h"

class IAppBuilder;

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
    bool operator<(const BroadphaseKey& rhs) const {
      return value < rhs.value;
    }
    size_t value{};
  };

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

  //This holds the user keys and boundaries of the shapes tracked by the broadphase
  //The spatial data structure references this to decide how to partition the objects
  struct ObjectDB {
    static constexpr size_t S = 2;
    static constexpr float NEW = std::numeric_limits<float>::min();
    static constexpr float REMOVED = std::numeric_limits<float>::max();
    static constexpr size_t EMPTY = std::numeric_limits<size_t>::max();
    static constexpr BroadphaseKey EMPTY_KEY{ EMPTY };
    using BoundsAxis = std::vector<std::pair<float, float>>;

    std::array<BoundsAxis, S> bounds;
    std::vector<UserKey> userKey;
    //Keys ready for re-use
    std::vector<BroadphaseKey> freeList;
    //Keys recently marked for removal but won't be moved to the free list until the next recomputePairs
    std::vector<BroadphaseKey> pendingRemoval;
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
}

template<>
struct std::hash<Broadphase::SweepCollisionPair> {
  std::size_t operator()(const Broadphase::SweepCollisionPair& s) const noexcept {
    std::hash<size_t> h;
    //cppreference hash combine example
    return h(s.a) ^ (h(s.b) << 1);
  }
};

namespace Broadphase {
  struct SwapLog {
    std::vector<SweepCollisionPair>& gains;
    std::vector<SweepCollisionPair>& losses;
  };

  //During a first pass, "candidates" are determined as pairs that potentially started or stopped colliding
  //In the second pass redundant entries are removed and they ar eturned into gains and losses
  struct CollisionCandidates {
    std::vector<SweepCollisionPair> pairs;
  };

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
    float min{};
    float max{};
  };

  struct Sweep2D {
    static constexpr size_t S = 2;
    std::array<SweepAxis, S> axis;
    std::unordered_set<BroadphaseKey> containedKeys;
    //TODO: somehow get around the need for this
    std::unordered_set<SweepCollisionPair> trackedPairs;
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

    void recomputeCandidates(Sweep2D& sweep, CollisionCandidates& candidates);
    void resolveCandidates(Sweep2D& sweep, CollisionCandidates& candidates, SwapLog log);
    void recomputePairs(Sweep2D& sweep, CollisionCandidates& candidates, SwapLog& log);
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
    struct Grid {
      ObjectDB objects;
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