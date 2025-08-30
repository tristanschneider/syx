#include "Precompile.h"

#include "SpatialQueries.h"

#include "TableAdapters.h"

#include "CommonTasks.h"
#include "Events.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_inverse.hpp"
#include "glm/gtx/norm.hpp"

#include "GameMath.h"
#include "Physics.h"
#include "SpatialPairsStorage.h"
#include <transform/TransformModule.h>
#include <TableName.h>

namespace SpatialQuery {
  //The wrapping types are used to indicate which part of the table is intended to be accessed by gameplay or physics
  //They make sense to be in the same table since the elements should always map to each other but due to multithreading
  //the appropriate side must make sure to only read from data appropriate for it.
  template<class T> struct Gameplay : T{};
  template<class T> struct Physics : T{};

  struct LifetimeRow : Row<size_t> {};
  //Set if this element needs to be rewritten from gameplay to physics
  struct NeedsResubmitRow : SparseFlagRow{};

  struct Globals {
    //Mutable is hack to be able to use this as const which makes the scheduler see it as parallel due to manual thread-safety
    //via mutex. Without it the scheduler will instead avoid overlapping tasks
    mutable std::vector<Command> commandBuffer;
    mutable std::mutex mutex;
  };
  struct GlobalsRow : SharedRow<Globals> {};

  struct QueryTransform {
    Transform::PackedTransform operator()(const Raycast& shape) const {
      return Shapes::toTransform(ShapeRegistry::Raycast{ .start = shape.start, .end = shape.end }, 0);
    }

    Transform::PackedTransform operator()(const AABB& shape) const {
      return Shapes::toTransform(ShapeRegistry::AABB{ .min = shape.min, .max = shape.max }, 0);
    }

    Transform::PackedTransform operator()(const Circle& shape) const {
      return Shapes::toTransform(ShapeRegistry::Circle{ .pos = shape.pos, .radius = shape.radius }, 0);
    }

    Raycast operator()(const Raycast&, const Transform::PackedTransform& transform) const {
      const ShapeRegistry::Raycast result = Shapes::lineFromTransform(transform);
      return Raycast{
        .start = result.start,
        .end = result.end
      };
    }

    AABB operator()(const AABB&, const Transform::PackedTransform& transform) const {
      const ShapeRegistry::AABB result = Shapes::aabbFromTransform(transform);
      return AABB{
        .min = result.min,
        .max = result.max
      };
    }

    Circle operator()(const Circle&, const Transform::PackedTransform& transform) const {
      const ShapeRegistry::Circle result = Shapes::circleFromTransform(transform);
      return Circle{
        .pos = result.pos,
        .radius = result.radius
      };
    }
  };

  struct VisitIsCollision {
    //Currently the manifold is only populated if they are overlapping. If this changes then positive overlap could be checked here
    bool operator()(const ContactXY&) const {
      return true;
    }

    bool operator()(const ContactZ&) const {
      return true;
    }
  };

  bool Result::isCollision() const {
    return std::visit(VisitIsCollision{}, contact);
  }

  struct SQShapeTables {
    SQShapeTables() = default;
    SQShapeTables(RuntimeDatabaseTaskBuilder& task)
      : aabb{ task.queryTables<SpatialQueriesTableTag, Gameplay<Shapes::AABBRow>>()[0] }
      , circle{ task.queryTables<SpatialQueriesTableTag, Gameplay<Shapes::CircleRow>>()[0] }
      , raycast{ task.queryTables<SpatialQueriesTableTag, Gameplay<Shapes::LineRow>>()[0] }
    {}

    TableID aabb, circle, raycast;
  };
  struct CreateData {
    CreateData() = default;
    CreateData(RuntimeDatabaseTaskBuilder& task, const TableID& table)
      : globals{ task.query<const Gameplay<GlobalsRow>>(table).tryGetSingletonElement() }
      , ids{ task.getIDResolver() }
    {}

    std::shared_ptr<IIDResolver> ids;
    const Globals* globals{};
  };

  struct WriteData {
    WriteData() = default;
    WriteData(RuntimeDatabaseTaskBuilder& task, TableID tid) {
      auto q = task.query<
        Gameplay<Transform::WorldTransformRow>,
        Gameplay<LifetimeRow>,
        Gameplay<NeedsResubmitRow>
      >(tid);
      table = tid;
      auto [s, l, n] = q.get(0);
      transforms = s.get();
      lifetimes = l.get();
      needsResubmit = n.get();
      ids = task.getIDResolver();
    }

    template<IsRow ShapeRow>
    static WriteData create(RuntimeDatabaseTaskBuilder& task) {
      //Arbitrary query to uniquely identify the query table
      return WriteData{ task, task.queryTables<Gameplay<LifetimeRow>, ShapeRow>().getTableID(0) };
    }

    UnpackedDatabaseElementID table;
    std::shared_ptr<IIDResolver> ids;
    Gameplay<Transform::WorldTransformRow>* transforms{};
    Gameplay<LifetimeRow>* lifetimes{};
    Gameplay<NeedsResubmitRow>* needsResubmit{};
  };

  ElementRef createQuery(CreateData& data, Query&& query, size_t lifetime) {
    //Immediately reserve the key, the entry will be created later
    ElementRef result = data.ids->createKey();
    {
      //Enqueue the command for the end of the frame.
      //The lock is for multiple tasks submitting requests at the same time which does not
      //overlap with the synchronous processing of commands at the end of the frame
      std::lock_guard<std::mutex> lock{ data.globals->mutex };
      data.globals->commandBuffer.push_back({ SpatialQuery::Command::NewQuery{ result, std::move(query), lifetime } });
    }
    return result;
  }

  void updateLifetime(WriteData& data, size_t index, size_t newLifetime) {
    data.needsResubmit->getOrAdd(index);
    data.lifetimes->at(index) = newLifetime;
  }

  template<class Op>
  void modifyQuery(WriteData& data, size_t index, Query& query, size_t newLifetime, Op&& op) {
    Transform::PackedTransform& dstTransform = data.transforms->at(index);
    std::visit([&](auto& srcShape) {
      //Convert stored transform to the same shape as the input query
      auto dstShape = QueryTransform{}(srcShape, dstTransform);
      //Presumably write to the temporary shape
      op(dstShape, srcShape);
      //Convert the changed shape back to a transform and store it
      dstTransform = QueryTransform{}(dstShape);
    }, query.shape);

    updateLifetime(data, index, newLifetime);
  }

  void refreshQuery(WriteData& data, size_t index, Query&& query, size_t newLifetime) {
    modifyQuery(data, index, query, newLifetime, [](auto& dst, auto& src) { dst = std::move(src); });
  }

  void swapQuery(WriteData& data, size_t index, Query& inout, size_t newLifetime) {
    modifyQuery(data, index, inout, newLifetime, [](auto& dst, auto& src) {
      auto temp = src;
      src = dst;
      dst = temp;
    });
  }

  void refreshQuery(WriteData& data, size_t index, size_t newLifetime) {
    //Lifetime updates by themselves don't need to set the resubmit flag because all of these are checked
    //anyway during the lifetime update
    data.lifetimes->at(index) = newLifetime;
  }

  struct Creator : ICreator {
    Creator(RuntimeDatabaseTaskBuilder& task) {
      SQShapeTables tables{ task };
      createAABB = CreateData{ task, tables.aabb };
      createCircle = CreateData{ task, tables.circle };
      createRaycast = CreateData{ task, tables.raycast };
    }

    ElementRef createQuery(Query&& query, size_t lifetime) override {
      if(auto aabb = std::get_if<AABB>(&query.shape)) {
        return SpatialQuery::createQuery(createAABB, std::move(query), lifetime);
      }
      else if(auto circle = std::get_if<Circle>(&query.shape)) {
        return SpatialQuery::createQuery(createCircle, std::move(query), lifetime);
      }
      else if(auto ray = std::get_if<Raycast>(&query.shape)) {
        return SpatialQuery::createQuery(createRaycast, std::move(query), lifetime);
      }
      return {};
    }

    CreateData createAABB, createCircle, createRaycast;
  };

  struct Reader : IReader {
    Reader(RuntimeDatabaseTaskBuilder& task) {
      graph = task.query<const SP::IslandGraphRow>().tryGetSingletonElement();
      auto q = task.query<const SP::ManifoldRow, const SP::ZManifoldRow, const SP::PairTypeRow>();
      auto [m, zm, pt] = q.get(0);
      manifold = m.get();
      zManifold = zm.get();
      pairTypes = pt.get();
      ids = task.getIDResolver();
    }

    //TODO: better iterator support
    //Store the first edge entry for this node in the graph
    void begin(const ElementRef& id) override {
      selectedEdgeEntry = IslandGraph::INVALID;

      self = id;
      if(!self) {
        return;
      }
      unpacked = ids->getRefResolver().uncheckedUnpack(self);

      auto it = graph->findNode(self);
      if(it != graph->nodesEnd()) {
        const IslandGraph::Node& node = graph->nodes[it.node];
        selectedEdgeEntry = node.edges;
      }
    }

    struct ResultBuilder {
      ResultBuilder(Result& r, const ElementRef& self, const IslandGraph::Graph& graph, const IslandGraph::Edge& edge)
        : result{ r }
      {
        const ElementRef& stableA = graph.nodes[edge.nodeA].data;
        const ElementRef& stableB = graph.nodes[edge.nodeB].data;
        if(stableA == self) {
          flipResults = false;
          result.other = stableB;
        }
        else {
          flipResults = true;
          result.other = stableA;
        }
      }

      void addResults(const SP::ContactManifold& manifold) {
        ContactXY& c = result.contact.emplace<ContactXY>();
        c.size = static_cast<uint8_t>(manifold.size);
        for(size_t i = 0; i < manifold.size; ++i) {
          c.points[i].point = flipResults ? manifold[i].centerToContactB : manifold[i].centerToContactA;
          c.points[i].normal = flipResults ? -manifold.points[i].normal : manifold[i].normal;
          c.points[i].overlap = manifold[i].overlap;
        }
      }

      void addResults(const SP::ZInfo& manifold) {
        ContactZ& c = result.contact.emplace<ContactZ>();
        c.normal = flipResults ? -manifold.normal : manifold.normal;
        c.overlap = manifold.separation;
      }

      Result& result;
      bool flipResults{};
    };

    //Return results from the current edge entry and advance to the next one
    const Result* tryIterate() override {
      //Iterate through edges until one is found that passed the narrowphase
      while(selectedEdgeEntry != IslandGraph::INVALID) {
        const IslandGraph::EdgeEntry& entry = graph->edgeEntries[selectedEdgeEntry];
        const IslandGraph::Edge& edge = graph->edges[entry.edge];
        selectedEdgeEntry = entry.nextEntry;

        //Should always work
        if(pairTypes->size() > edge.data) {
          const SP::PairType pairType = pairTypes->at(edge.data);
          ResultBuilder builder{ cachedResult, self, *graph, edge };
          //If there are points, copy them over. If there aren't then this made it past broadphase but not narrowphase
          switch(pairType) {
            case SP::PairType::Constraint:
              //Currently not exposed for iteration
              break;
            case SP::PairType::ContactXY: {
              const SP::ContactManifold& man = manifold->at(edge.data);
              if(man.size) {
                builder.addResults(man);
                return &cachedResult;
              }
              break;
            }
            case SP::PairType::ContactZ: {
              const SP::ZContactManifold& zman = zManifold->at(edge.data);
              if(zman.isTouching()) {
                builder.addResults(zman.info);
                return &cachedResult;
              }
              break;
            }
          }
        }
      }
      return nullptr;
    }

    const IslandGraph::Graph* graph{};
    const SP::PairTypeRow* pairTypes{};
    const SP::ManifoldRow* manifold{};
    const SP::ZManifoldRow* zManifold{};
    std::shared_ptr<IIDResolver> ids{};

    uint32_t selectedEdgeEntry{ IslandGraph::INVALID };
    UnpackedDatabaseElementID unpacked;
    ElementRef self;
    Result cachedResult;
  };

  struct Writer : IWriter {
    Writer(RuntimeDatabaseTaskBuilder& task)
      : writeAABB{ WriteData::create<Shapes::AABBRow>(task) }
      , writeCircle{ WriteData::create<Shapes::CircleRow>(task) }
      , writeRay{ WriteData::create<Shapes::LineRow>(task) }
    {
    }

    std::optional<UnpackedDatabaseElementID> getKey(const ElementRef& id) {
      return writeAABB.ids->getRefResolver().tryUnpack(id);
    }

    template<class T>
    void visitShape(const UnpackedDatabaseElementID& key, const T& visitor) {
      if(key.getTableIndex() == writeAABB.table.getTableIndex()) {
        visitor(writeAABB);
      }
      else if(key.getTableIndex() == writeCircle.table.getTableIndex()) {
        visitor(writeCircle);
      }
      else if(key.getTableIndex() == writeRay.table.getTableIndex()) {
        visitor(writeRay);
      }
    }

    void swapQuery(const UnpackedDatabaseElementID& key, Query& inout, size_t newLifetime) override {
      visitShape(key, [&](auto& shape) { SpatialQuery::swapQuery(shape, key.getElementIndex(), inout, newLifetime); });
    }

    void refreshQuery(const UnpackedDatabaseElementID& key, Query&& query, size_t newLifetime) override {
      visitShape(key, [&](auto& shape) { SpatialQuery::refreshQuery(shape, key.getElementIndex(), std::move(query), newLifetime); });
    }

    void refreshQuery(const UnpackedDatabaseElementID& key, size_t newLifetime) override {
      visitShape(key, [&](auto& shape) { SpatialQuery::refreshQuery(shape, key.getElementIndex(), newLifetime); });
    }

    void refreshQuery(const ElementRef& key, Query&& query, size_t newLifetime) override {
      if(auto k = getKey(key)) {
        refreshQuery(*k, std::move(query), newLifetime);
      }
    }

    void refreshQuery(const ElementRef& key, size_t newLifetime) override {
      if(auto k = getKey(key)) {
        refreshQuery(*k, newLifetime);
      }
    }

    WriteData writeAABB;
    WriteData writeCircle;
    WriteData writeRay;
  };

  std::shared_ptr<ICreator> createCreator(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<Creator>(task);
  }

  std::shared_ptr<IReader> createReader(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<Reader>(task);
  }

  std::shared_ptr<IWriter> createWriter(RuntimeDatabaseTaskBuilder& task) {
    return std::make_shared<Writer>(task);
  }

  void processLifetime(IAppBuilder& builder, const TableID& table) {
    auto task = builder.createTask();
    task.setName("SQ lifetimes");
    Globals* globals = task.query<Gameplay<GlobalsRow>>(table).tryGetSingletonElement();
    auto lifetimes = task.query<Gameplay<LifetimeRow>, const StableIDRow>(table);
    auto ids = task.getIDResolver();
    assert(globals && lifetimes.size());
    task.setCallback([ids, globals, lifetimes](AppTaskArgs&) mutable {
      for(size_t i = 0; i < lifetimes.get<0>(0).size(); ++i) {
        size_t& lifetime = lifetimes.get<0>(0).at(i);
        if(!lifetime) {
          const ElementRef id = lifetimes.get<1>(0).at(i);
          //Not necessary at the moment since it's synchronous but would be if this task was made to operate on ranges
          std::lock_guard<std::mutex> lock{ globals->mutex };
          globals->commandBuffer.push_back({ Command::DeleteQuery{ id } });
        }
        else {
          --lifetime;
        }
      }
    });
    builder.submitTask(std::move(task));
  }

  void submitQueryUpdates(IAppBuilder& builder, const TableID& table) {
    if(!builder.queryTable<Gameplay<Transform::WorldTransformRow>>(table)) {
      return;
    }
    auto task = builder.createTask();
    task.setName("Spatial query updates");
    auto query = task.query<
      const Gameplay<Transform::WorldTransformRow>,
      Transform::WorldTransformRow,
      Transform::TransformNeedsUpdateRow,
      Gameplay<NeedsResubmitRow>
    >(table);
    task.setCallback([query](AppTaskArgs&) mutable {
      auto&& [ gameplay, physics, transformUpdate, needsResubmit ] = query.get(0);
      for(size_t i : needsResubmit) {
        physics->at(i) = gameplay->at(i);
        transformUpdate->getOrAdd(i);
      }
      needsResubmit->clear();
    });
    builder.submitTask(std::move(task));
  }

  //TODO: use thread-local database to create these
  struct CommandVisitor {
    void operator()(const Command::NewQuery& command) {
      const size_t index = gameplayQueries.size();
      modifier.resizeWithIDs(index + 1, &command.id);
      //Add to physics and gameplay locations now
      //Using the NeedsResubmitRow not feasible here because it's after that has already been processed
      //The appropriate shapes were dispatched to this table so at this point the variant should only have these
      Transform::PackedTransform transform = std::visit(QueryTransform{}, command.query.shape);
      gameplayQueries.at(index) = transform;
      //No need to set TransformNeedsUpdate because new elements are automatically flagged for update
      physicsQueries.at(index) = transform;
      gameplayLifetimes.at(index) = command.lifetime;
      events.getOrAdd(index).setCreate();
    }

    void operator()(const Command::DeleteQuery& command) {
      //Publish the removal event which will cause removal from the broadphase and removal from the table
      if(auto i = ids.unpack(command.id)) {
        events.getOrAdd(i.getElementIndex()).setDestroy();
      }
    }

    ITableModifier& modifier;
    Gameplay<Transform::WorldTransformRow>& gameplayQueries;
    Gameplay<LifetimeRow>& gameplayLifetimes;
    Transform::WorldTransformRow& physicsQueries;
    Events::EventsRow& events;
    ElementRefResolver ids;
  };

  void processCommandBuffer(IAppBuilder& builder, const TableID& table) {
    if(!builder.queryTable<Gameplay<GlobalsRow>>(table)) {
      return;
    }
    auto task = builder.createTask();
    task.setName("SQ commands");
    auto modifier = task.getModifierForTable(table);
    auto query = task.query<
      Gameplay<GlobalsRow>,
      Gameplay<Transform::WorldTransformRow>,
      Gameplay<LifetimeRow>,
      Transform::WorldTransformRow,
      Events::EventsRow
    >(table);
    ElementRefResolver ids = task.getIDResolver()->getRefResolver();

    task.setCallback([ids, query, modifier](AppTaskArgs&)  mutable {
      CommandVisitor visitor {
        *modifier,
        *query.getSingleton<1>(),
        *query.getSingleton<2>(),
        *query.getSingleton<3>(),
        *query.getSingleton<4>(),
        ids
      };

      Globals& globals = query.getSingleton<0>()->at();
      {
        std::lock_guard<std::mutex> lock{ globals.mutex };
        for(auto&& cmd : globals.commandBuffer) {
          std::visit(visitor, cmd.data);
        }
        globals.commandBuffer.clear();
      }
    });

    builder.submitTask(std::move(task));
  }

  void gameplayUpdateQueries(IAppBuilder& builder) {
    auto tables = builder.queryTables<SpatialQueriesTableTag>();
    for(size_t i = 0; i < tables.size(); ++i) {
      const TableID& table = tables[i];
      processLifetime(builder, table);
      submitQueryUpdates(builder, table);
      processCommandBuffer(builder, table);
    }
  }

  StorageTableBuilder createBaseQueryTable() {
    StorageTableBuilder table;
    table.addRows<
      SpatialQueriesTableTag,
      SweepNPruneBroadphase::BroadphaseKeys,
      //Gameplay writes to this then updates are submitted to the actual transform row added by Transform::addTransform25D
      Gameplay<Transform::WorldTransformRow>,
      Gameplay<LifetimeRow>,
      Gameplay<NeedsResubmitRow>,
      Gameplay<GlobalsRow>,
      //Collision mask row to detect collisions, no constraint row since solving isn't desired
      Narrowphase::CollisionMaskRow
    >().setStable();
    Transform::addTransform25D(table);
    return table;
  }

  template<IsRow ShapeRow>
  StorageTableBuilder createShapeQueryTable(std::string_view name) {
    StorageTableBuilder result = createBaseQueryTable();
    result.addRows<ShapeRow>().setTableName({ std::string{ name } });
    return result;
  }

  void createSpatialQueryTables(RuntimeDatabaseArgs& args) {
    createShapeQueryTable<Shapes::AABBRow>("AABBQueries").finalize(args);
    createShapeQueryTable<Shapes::CircleRow>("CircleQueries").finalize(args);
    createShapeQueryTable<Shapes::LineRow>("LineQueries").finalize(args);
  }
}