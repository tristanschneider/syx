#include "Precompile.h"

#include "SpatialQueries.h"

#include "TableAdapters.h"

#include "CommonTasks.h"
#include "DBEvents.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_inverse.hpp"
#include "glm/gtx/norm.hpp"

#include "GameMath.h"
#include "Physics.h"
#include "SpatialPairsStorage.h"

namespace SpatialQuery {
  template<class ShapeRow>
  struct ShapeID {};

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


  template<class Callback>
  void visitShapes(const Callback& cb) {
    cb(ShapeID<Shapes::AABBRow>{});
    cb(ShapeID<Shapes::CircleRow>{});
    cb(ShapeID<Shapes::LineRow>{});
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
  template<class ShapeRow>
  struct WriteData {
    WriteData() = default;
    WriteData(RuntimeDatabaseTaskBuilder& task) {
      auto q = task.query<
        Gameplay<ShapeRow>,
        Gameplay<LifetimeRow>,
        Gameplay<NeedsResubmitRow>
      >();
      table = q.matchingTableIDs[0];
      std::tie(shapes, lifetimes, needsResubmit) = q.get(0);
      ids = task.getIDResolver();
    }
    UnpackedDatabaseElementID table;
    std::shared_ptr<IIDResolver> ids;
    Gameplay<ShapeRow>* shapes{};
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

  template<class QueryShape>
  constexpr size_t indexOfNarrowphaseShape() {
    //TODO: probably a nicer thing I could do here
    if constexpr(std::is_same_v<Narrowphase::Shape::AABB, QueryShape>) {
      return 1;
    }
    else if constexpr(std::is_same_v<Narrowphase::Shape::Circle, QueryShape>) {
      return 2;
    }
    else if constexpr(std::is_same_v<Narrowphase::Shape::Raycast, QueryShape>) {
      return 0;
    }
  }

  template<class T, class Op>
  void modifyQuery(WriteData<T>& data, size_t index, Query& query, size_t newLifetime, Op&& op) {
    using ShapeT = typename T::ElementT;
    constexpr size_t INDEX = indexOfNarrowphaseShape<ShapeT>();
    assert(query.shape.index() == INDEX && "Query shape must stay the same");
    op(data.shapes->at(index), std::get<INDEX>(query.shape));
    data.needsResubmit->at(index) = true;
    data.lifetimes->at(index) = newLifetime;
  }

  template<class T>
  void refreshQuery(WriteData<T>& data, size_t index, Query&& query, size_t newLifetime) {
    modifyQuery(data, index, query, newLifetime, [](auto& dst, auto& src) { dst = std::move(src); });
  }

  template<class T>
  void swapQuery(WriteData<T>& data, size_t index, Query& inout, size_t newLifetime) {
    modifyQuery(data, index, inout, newLifetime, [](auto& dst, auto& src) {
      auto temp = src;
      src = dst;
      dst = temp;
    });
  }

  template<class T>
  void refreshQuery(WriteData<T>& data, size_t index, size_t newLifetime) {
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
      std::tie(manifold, zManifold, pairTypes) = task.query<const SP::ManifoldRow, const SP::ZManifoldRow, const SP::PairTypeRow>().get(0);
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
      : writeAABB{ task }
      , writeCircle{ task }
      , writeRay{ task }
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

    WriteData<Shapes::AABBRow> writeAABB;
    WriteData<Shapes::CircleRow> writeCircle;
    WriteData<Shapes::LineRow> writeRay;
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

  template<class ShapeRow>
  void submitQueryUpdates(ShapeID<ShapeRow>, IAppBuilder& builder, const TableID& table) {
    if(!builder.queryTable<ShapeRow>(table)) {
      return;
    }
    auto task = builder.createTask();
    task.setName("Spatial query updates");
    auto query = task.query<
      const Gameplay<ShapeRow>,
      ShapeRow,
      Gameplay<NeedsResubmitRow>
    >(table);
    task.setCallback([query](AppTaskArgs&) mutable {
      auto&& [ gameplay, physics, needsResubmit ] = query.get(0);
      for(size_t i = 0; i < physics->size(); ++i) {
        //This assumes resubmits are frequent.
        //If not, it may be faster to add a command to point at the index of what needs to be resubmitted
        if(needsResubmit->at(i)) {
          needsResubmit->at(i) = false;
          physics->at(i) = gameplay->at(i);
        }
      }
    });
    builder.submitTask(std::move(task));
  }

  template<class ShapeRow>
  struct CommandVisitor {
    using ShapeT = typename ShapeRow::ElementT;
    void operator()(const Command::NewQuery& command) {
      const size_t index = gameplayQueries.size();
      modifier.resizeWithIDs(index + 1, &command.id);
      constexpr size_t INDEX = indexOfNarrowphaseShape<ShapeT>();
      //Add to physics and gameplay locations now
      //Using the NeedsResubmitRow not feasible here because it's after that has already been processed
      //The appropriate shapes were dispatched to this table so at this point the variant should only have these
      gameplayQueries.at(index) = std::get<INDEX>(command.query.shape);
      physicsQueries.at(index) = std::get<INDEX>(command.query.shape);
      gameplayLifetimes.at(index) = command.lifetime;
      publishNewElement(command.id);
    }

    void operator()(const Command::DeleteQuery& command) {
      //Publish the removal event which will cause removal from the broadphase and removal from the table
      publishRemovedElement(command.id);
    }

    ITableModifier& modifier;
    Gameplay<ShapeRow>& gameplayQueries;
    Gameplay<LifetimeRow>& gameplayLifetimes;
    ShapeRow& physicsQueries;

    Events::CreatePublisher& publishNewElement;
    Events::DestroyPublisher& publishRemovedElement;
    IIDResolver& ids;
  };

  template<class ShapeRow>
  void processCommandBuffer(ShapeID<ShapeRow>, IAppBuilder& builder, const TableID& table) {
    if(!builder.queryTable<ShapeRow>(table)) {
      return;
    }
    auto task = builder.createTask();
    task.setName("SQ commands");
    auto modifier = task.getModifierForTable(table);
    auto query = task.query<
      Gameplay<GlobalsRow>,
      Gameplay<ShapeRow>,
      Gameplay<LifetimeRow>,
      ShapeRow
    >(table);
    auto ids = task.getIDResolver();

    task.setCallback([ids, query, modifier](AppTaskArgs& args)  mutable {
      Events::CreatePublisher create({ &args });
      Events::DestroyPublisher destroy({ &args });
      CommandVisitor visitor {
        *modifier,
        *query.getSingleton<1>(),
        *query.getSingleton<2>(),
        *query.getSingleton<3>(),
        create,
        destroy,
        *ids
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
      const TableID& table = tables.matchingTableIDs[i];
      processLifetime(builder, table);
      visitShapes([&](auto shape) {
        submitQueryUpdates(shape, builder, table);
        processCommandBuffer(shape, builder, table);
      });
    }
  }
}