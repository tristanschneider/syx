#pragma once

#include "glm/vec2.hpp"
#include "StableElementID.h"
#include "SweepNPruneBroadphase.h"
#include "Table.h"
#include "Physics.h"
#include "Narrowphase.h"
#include "shapes/AABB.h"
#include "shapes/Circle.h"
#include "shapes/Rectangle.h"
#include "shapes/Line.h"

class IAppBuilder;
class RuntimeDatabaseTaskBuilder;

struct GameDB;
struct SpatialQueryAdapter;
struct TaskRange;

namespace SpatialQuery {
  //Single use query will be created at the end of the tick,
  //Viewable the tick after, then destroyed at the end of it
  constexpr static size_t SINGLE_USE = 0;

  struct RaycastResult {
    ElementRef id;
    glm::vec2 point{};
    //In world space, not normalized
    glm::vec2 normal{};
  };

  struct RaycastResults {
    std::vector<RaycastResult> results;
  };

  struct Raycast {
    operator Narrowphase::Shape::Raycast() const {
      return { start, end };
    }
    Raycast& operator=(const Narrowphase::Shape::Raycast& rhs) {
      start = rhs.start;
      end = rhs.end;
      return *this;
    }
    glm::vec2 start{};
    glm::vec2 end{};
  };

  struct VolumeResult {
    ElementRef id;
  };
  struct AABB {
    operator Narrowphase::Shape::AABB() const {
      return { min, max };
    }
    AABB& operator=(const Narrowphase::Shape::AABB& rhs) {
      min = rhs.min;
      max = rhs.max;
      return *this;
    }
    glm::vec2 min{};
    glm::vec2 max{};
  };
  struct Circle {
    operator Narrowphase::Shape::Circle() const {
      return { pos, radius };
    }
    Circle& operator=(const Narrowphase::Shape::Circle& rhs) {
      pos = rhs.pos;
      radius = rhs.radius;
      return *this;
    }
    glm::vec2 pos{};
    float radius{};
  };

  struct VolumeResults {
    std::vector<VolumeResult> results;
  };
  struct AABBResults : VolumeResults {};
  struct CircleResults : VolumeResults {};

  struct Query {
    using Shape = std::variant<Raycast, AABB, Circle>;
    Shape shape;
  };

  struct ContactPoint {
    glm::vec2 point{};
    glm::vec2 normal{};
    float overlap{};
  };
  struct ContactXY {
    std::array<ContactPoint, 2> points;
    uint8_t size{};
  };
  struct ContactZ {
    float normal{};
    float overlap{};
  };

  struct Result {
    bool isCollision() const;

    ElementRef other;
    using Variant = std::variant<ContactXY, ContactZ>;
    Variant contact;
  };

  struct LifetimeRow : Row<size_t> {};
  //True if this element needs to be rewritten from gameplay to physics
  struct NeedsResubmitRow : Row<uint8_t> {};

  struct Command {
    struct NewQuery {
      ElementRef id;
      Query query;
      size_t lifetime{};
    };
    struct DeleteQuery {
      ElementRef id;
    };
    using Variant = std::variant<NewQuery, DeleteQuery>;
    Variant data;
  };

  struct Globals {
    //Mutable is hack to be able to use this as const which makes the scheduler see it as parallel due to manual thread-safety
    //via mutex. Without it the scheduler will instead avoid overlapping tasks
    mutable std::vector<Command> commandBuffer;
    mutable std::mutex mutex;
  };
  struct GlobalsRow : SharedRow<Globals> {};

  //The wrapping types are used to indicate which part of the table is intended to be accessed by gameplay or physics
  //They make sense to be in the same table since the elements should always map to each other but due to multithreading
  //the appropriate side must make sure to only read from data appropriate for it.
  template<class T> struct Gameplay : T{};
  template<class T> struct Physics : T{};

  struct MinX : Row<float> {};
  struct MaxX : Row<float> {};
  struct MinY : Row<float> {};
  struct MaxY : Row<float> {};

  template<class ShapeRow>
  struct SpatialQueriesTableT : Table<
    SpatialQueriesTableTag,
    //Conceptually this is Physics<ShapeRow> but Physics does not allow an alias for this row type so it must be exact
    ShapeRow,
    SweepNPruneBroadphase::BroadphaseKeys,

    Gameplay<ShapeRow>,
    Gameplay<LifetimeRow>,
    Gameplay<NeedsResubmitRow>,
    Gameplay<GlobalsRow>,
    StableIDRow,
    //Collision mask row to detect collisions, no constraint row since solving isn't desired
    Narrowphase::CollisionMaskRow
  > {};

  struct AABBSpatialQueriesTable : SpatialQueriesTableT<Shapes::AABBRow> {};
  struct CircleSpatialQueriesTable : SpatialQueriesTableT<Shapes::CircleRow> {};
  struct RaycastSpatialQueriesTable : SpatialQueriesTableT<Shapes::LineRow> {};

  //Physics and gameplay both have their sides of the spatial query table
  //Gameplay modifies its side and keeps track of a queue of new entries to submit
  //By the end of the frame physics has written all the results to its version of the table
  //Gameplay ticks lifetimes of all queries on its side, enqueueing a removal request for any that have expired
  //Gameplay copies the physics results over to its own side
  //Next, gameplay submits all updates to the physics table which includes updating the values of the queries as well as adding and removing entries

  //Physics processes the requests by leveraging the normal broadphase flow.
  //This means the DBEvents for add and remove will automatically add the queries to the broadphase
  //physicsUpdateBoundaries runs before the broadphase update to update the boundaries that will be written to the broadphase
  //physicsProcessQueries then refines the output collision pairs to ensure they fit in the actual volume

  struct ICreator {
    virtual ~ICreator() = default;
    //Enqueues creation of the query whose results will be available in two ticks. At the beginning of next tick the query will be submitted to physics
    //and the tick after that the results will be extracted to gameplay
    //The query is not submitted instantly but the stable id is reserved immediately. The ID is pointing at the gameplay extracted version
    virtual ElementRef createQuery(Query&& query, size_t lifetime) = 0;
  };
  struct IReader {
    virtual ~IReader() = default;
    //Select the spatial query to iterate over
    virtual void begin(const ElementRef& id) = 0;
    //Iterate over the selected query
    virtual const Result* tryIterate() = 0;
  };
  struct IWriter {
    virtual ~IWriter() = default;

    virtual std::optional<UnpackedDatabaseElementID> getKey(const ElementRef& id) = 0;
    virtual void refreshQuery(const UnpackedDatabaseElementID& key, Query&& query, size_t newLifetime) = 0;
    //Same as refresh but swaps the old one into the parameter
    virtual void swapQuery(const UnpackedDatabaseElementID& key, Query& inout, size_t newLifetime) = 0;
    virtual void refreshQuery(const UnpackedDatabaseElementID& key, size_t newLifetime) = 0;
    virtual void refreshQuery(const ElementRef& key, Query&& query, size_t newLifetime) = 0;
    virtual void refreshQuery(const ElementRef& index, size_t newLifetime) = 0;
  };

  std::shared_ptr<ICreator> createCreator(RuntimeDatabaseTaskBuilder& task);
  std::shared_ptr<IReader> createReader(RuntimeDatabaseTaskBuilder& task);
  std::shared_ptr<IWriter> createWriter(RuntimeDatabaseTaskBuilder& task);

  //Update boundaries based on query shape before they are updated in the broadphase
  void physicsUpdateBoundaries(IAppBuilder& builder);
  //Ticks gameplay query lifetimes, copies new results from physics and submits new add/remove requests to physics
  void gameplayUpdateQueries(IAppBuilder& builder);
};