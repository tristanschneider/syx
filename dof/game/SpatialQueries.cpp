#include "Precompile.h"

#include "SpatialQueries.h"

#include "TableAdapters.h"
#include "Simulation.h"

#include "CommonTasks.h"
#include "DBEvents.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_inverse.hpp"
#include "glm/gtx/norm.hpp"

#include "GameMath.h"
#include "Physics.h"

namespace SpatialQuery {
  struct CreateData {
    std::shared_ptr<IIDResolver> ids;
    const Globals* globals{};
  };
  struct ReadData {
    std::shared_ptr<IIDResolver> ids;
    const Gameplay<ResultRow>* results{};
  };
  struct WriteData {
    std::shared_ptr<IIDResolver> ids;
    Gameplay<QueryRow>* queries{};
    Gameplay<LifetimeRow>* lifetimes{};
    Gameplay<NeedsResubmitRow>* needsResubmit{};
  };

  StableElementID createQuery(CreateData& data, Query&& query, size_t lifetime) {
    //Immediately reserve the key, the entry will be created later
    StableElementID result = data.ids->createKey();
    {
      //Enqueue the command for the end of the frame.
      //The lock is for multiple tasks submitting requests at the same time which does not
      //overlap with the synchronous processing of commands at the end of the frame
      std::lock_guard<std::mutex> lock{ data.globals->mutex };
      data.globals->commandBuffer.push_back({ SpatialQuery::Command::NewQuery{ result, std::move(query), lifetime } });
    }
    return result;
  }

  std::optional<size_t> getIndex(const IIDResolver& resolver, StableElementID& id) {
    if(std::optional<StableElementID> resolved = resolver.tryResolveStableID(id)) {
      //Write down for later
      id = *resolved;
      //Return raw index that can be used directly into rows in the table
      return resolver.uncheckedUnpack(id).getElementIndex();
    }
    return {};
  }

  std::optional<size_t> getIndex(ReadData& data, StableElementID& id) {
    return getIndex(*data.ids, id);
  }

  void refreshQuery(WriteData& data, size_t index, size_t newLifetime) {
    //Lifetime updates by themselves don't need to set the resubmit flag because all of these are checked
    //anyway during the lifetime update
    data.lifetimes->at(index) = newLifetime;
  }

  void refreshQuery(WriteData& data, size_t index, Query&& query, size_t newLifetime) {
    data.queries->at(index) = std::move(query);
    data.needsResubmit->at(index) = true;
    refreshQuery(data, index, newLifetime);
  }

  void refreshQuery(WriteData& data, StableElementID& index, Query&& query, size_t newLifetime) {
    if(auto i = getIndex(*data.ids, index)) {
      refreshQuery(data, *i, std::move(query), newLifetime);
    }
  }

  void refreshQuery(WriteData& data, StableElementID& index, size_t newLifetime) {
    if(auto i = getIndex(*data.ids, index)) {
      refreshQuery(data, *i, newLifetime);
    }
  }

  const Result& getResult(ReadData& data, size_t index) {
    return data.results->at(index);
  }

  const Result* tryGetResult(ReadData& data, StableElementID& id) {
    auto i = getIndex(data, id);
    return i ? &getResult(data, *i) : nullptr;
  }

  struct Creator : ICreator {
    Creator(RuntimeDatabaseTaskBuilder& task) {
      data.globals = task.query<const Gameplay<GlobalsRow>>().tryGetSingletonElement();
      data.ids = task.getIDResolver();
    }

    StableElementID createQuery(Query&& query, size_t lifetime) override {
      return SpatialQuery::createQuery(data, std::move(query), lifetime);
    }

    CreateData data;
  };

  struct Reader : IReader {
    Reader(RuntimeDatabaseTaskBuilder& task) {
      data.results = task.query<const Gameplay<ResultRow>>().getSingleton<0>();
      data.ids = task.getIDResolver();
    }

    std::optional<size_t> getIndex(StableElementID& id) override {
      return SpatialQuery::getIndex(data, id);
    }

    const Result& getResult(size_t index) override {
      return SpatialQuery::getResult(data, index);
    }

    const Result* tryGetResult(StableElementID& id) override {
      return SpatialQuery::tryGetResult(data, id);
    }

    ReadData data;
  };

  struct Writer : IWriter {
    Writer(RuntimeDatabaseTaskBuilder& task) {
      data.queries = task.query<Gameplay<QueryRow>>().getSingleton<0>();
      data.lifetimes = task.query<Gameplay<LifetimeRow>>().getSingleton<0>();
      data.needsResubmit = task.query<Gameplay<NeedsResubmitRow>>().getSingleton<0>();
      data.ids = task.getIDResolver();
    }

    std::optional<size_t> getIndex(StableElementID& id) override {
      return SpatialQuery::getIndex(*data.ids, id);
    }

    void refreshQuery(size_t index, Query&& query, size_t newLifetime) override {
      SpatialQuery::refreshQuery(data, index, std::move(query), newLifetime);
    }

    void refreshQuery(size_t index, size_t newLifetime) override {
      SpatialQuery::refreshQuery(data, index, newLifetime);
    }

    void refreshQuery(StableElementID& index, Query&& query, size_t newLifetime) override {
      SpatialQuery::refreshQuery(data, index, std::move(query), newLifetime);
    }

    void refreshQuery(StableElementID& index, size_t newLifetime) override {
      SpatialQuery::refreshQuery(data, index, newLifetime);
    }

    WriteData data;
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

  struct BoundaryVisitor {
    void operator()(const Raycast& shape) {
      const glm::vec2 min = glm::min(shape.start, shape.end);
      const glm::vec2 max = glm::max(shape.start, shape.end);
      TableAdapters::write(index, min, minX, minY);
      TableAdapters::write(index, max, maxX, maxY);
    }

    void operator()(const AABB& shape) {
      TableAdapters::write(index, shape.min, minX, minY);
      TableAdapters::write(index, shape.max, maxX, maxY);
    }

    void operator()(const Circle& shape) {
      const glm::vec2 r{ shape.radius, shape.radius };
      TableAdapters::write(index, shape.pos - r, minX, minY);
      TableAdapters::write(index, shape.pos + r, maxX, maxY);
    }

    size_t index;
    MinX& minX;
    MinY& minY;
    MaxX& maxX;
    MaxY& maxY;
  };

  void physicsUpdateBoundaries(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("SQ boundaries");
    auto query = task.query<
      const Physics<QueryRow>,
      Physics<MinX>,
      Physics<MinY>,
      Physics<MaxX>,
      Physics<MaxY>
    >();
    task.setCallback([query](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto rows = query.get(t);
        BoundaryVisitor visitor {
          size_t{ 0 },
          *std::get<1>(rows),
          *std::get<2>(rows),
          *std::get<3>(rows),
          *std::get<4>(rows)
        };

        const Physics<QueryRow>* queries = std::get<0>(rows);
        for(size_t i = 0; i < queries->size(); ++i) {
          visitor.index = i;
          std::visit(visitor, queries->at(i).shape);
        }
      }
    });
    builder.submitTask(std::move(task));
  }

  struct ObjInfo {
    StableElementID id;
    glm::vec2 pos{};
    glm::vec2 rot{};
  };

  struct VisitPhysicsGain {
    bool operator()(const Raycast& raycast) {
      const glm::mat3 transform = Math::buildTransform(obj->pos, obj->rot, Math::getFragmentScale());
      const glm::mat3 invTransform = glm::affineInverse(transform);
      const glm::vec2 localStart = Math::transformPoint(invTransform, raycast.start);
      const glm::vec2 localEnd = Math::transformPoint(invTransform, raycast.end);
      const glm::vec2 dir = localEnd - localStart;
      float tmin, tmax;
      if(Math::unitAABBLineIntersect(localStart, dir, &tmin, &tmax)) {
        const glm::vec2 localPoint = localStart + dir*tmin;
        const glm::vec2 localNormal = Math::getNormalFromUnitAABBIntersect(localPoint);
        RaycastResult& r = getOrCreate<RaycastResults>(results->result).results.emplace_back();
        r.id = obj->id;
        r.point = Math::transformPoint(transform, localPoint);
        r.normal = Math::transformVector(transform, localNormal);
        return true;
      }
      return false;
    }

    bool operator()(const AABB&) {
      //Since the broadphase already did an AABB test add the results directly.
      //This will overshoot a bit due to padding, but it should be good enough
      getOrCreate<AABBResults>(results->result).results.push_back({ obj->id });
      return true;
    }

    bool operator()(const Circle& circle) {
      const glm::vec2 right = obj->rot;
      const glm::vec2 up = Math::orthogonal(right);
      constexpr glm::vec2 extents = Math::getFragmentExtents();
      //Get closest point on square to sphere by projecting onto each axis and clamping
      //Right and up are unit length so the projection doesn't need the denominator
      const glm::vec2 toSphere = circle.pos - obj->pos;
      const glm::vec2 closestOnSquare = right*glm::clamp(glm::dot(right, toSphere), -extents.x, extents.x) +
        up*glm::clamp(glm::dot(up, toSphere), -extents.y, extents.y);
      //See if closest point on square is within radius, meaning the circle is touching the square
      //len(cpos - (closest + pos))
      //toSphere = cpos - pos
      //cpos = toSphere - pos
      //len(toSphere - pos - (closest + pos))
      //len(toSphere - closest)
      if(glm::distance2(toSphere, closestOnSquare) <= circle.radius*circle.radius) {
        getOrCreate<CircleResults>(results->result).results.push_back({ obj->id });
        return true;
      }
      return false;
    }

    template<class T, class V>
    static T& getOrCreate(V& variant) {
      if(T* result = std::get_if<T>(&variant)) {
        return *result;
      }
      return variant.emplace<T>();
    }

    Result* results{};
    const ObjInfo* obj{};
  };


  struct PhysicsProcessArgs {
    IIDResolver& ids;
    ITableResolver& objResolver;
    const Physics<QueryRow>& physicsQueries;
    Physics<ResultRow>& physicsResults;
    const StableIDRow& stableRow;
  };

  //Copies the new pairs into the query `nearbyObjects` list which is then used in physicsProcessUpdates to see if they actually match
  void physicsProcessGains(PhysicsProcessArgs& args, const std::vector<SweepNPruneBroadphase::SpatialQueryPair>& pairs) {
    for(const auto& pair : pairs) {
      if(auto q = args.ids.tryResolveStableID(pair.query)) {
        args.physicsResults.at(args.ids.uncheckedUnpack(*q).getElementIndex()).nearbyObjects.push_back(pair.object);
      }
    }
  }

  //Removes from the `nearbyObjects` list which will prevent these from being put in the results list during physicsProcessUpdates
  void physicsProcessLosses(PhysicsProcessArgs& args, const std::vector<SweepNPruneBroadphase::SpatialQueryPair>& pairs) {
    for(const auto& pair : pairs) {
      if(auto q = args.ids.tryResolveStableID(pair.query)) {
        std::vector<StableElementID>& nearby = args.physicsResults.at(args.ids.uncheckedUnpack(*q).getElementIndex()).nearbyObjects;
        //TODO: probably slow if volumes are too big
        if(auto it = std::find_if(nearby.begin(), nearby.end(), StableElementFind{ pair.object }); it != nearby.end()) {
          //Swap remove
          *it = nearby.back();
          nearby.pop_back();
        }
      }
    }
  }


  //Iterates over all existing `nearbyObjects` and makes sure that they are still colliding
  //Objects are added and removed from that container through gains and losses, so the only results that need to be populated are here
  void physicsProcessUpdates(PhysicsProcessArgs& args) {
    auto clearResults = [](auto& r) { r.results.clear(); };
    using namespace Tags;
    CachedRow<const FloatRow<GPos, X>> px;
    CachedRow<const FloatRow<GPos, Y>> py;
    CachedRow<const FloatRow<GRot, CosAngle>> rx;
    CachedRow<const FloatRow<GRot, SinAngle>> ry;

    for(size_t i = 0; i < args.physicsResults.size(); ++i) {
      Result& r = args.physicsResults.at(i);
      //Clear the results completely and repopulate in the list below
      std::visit(clearResults, r.result);

      const Query& query = args.physicsQueries.at(i);
      VisitPhysicsGain visitor;
      visitor.results = &r;

      //Check all nearby to see if they're still colliding
      //If so they will refill the results list
      //If not, nothing happens, as the list was cleared above so they won't be in the results
      for(size_t o = 0; o < r.nearbyObjects.size(); ++o) {
        StableElementID& nearby = r.nearbyObjects[o];
        if(const auto obj = args.ids.tryResolveStableID(nearby)) {
          //Update mapping
          nearby = *obj;

          //Look up object
          const UnpackedDatabaseElementID rawObj = args.ids.uncheckedUnpack(*obj);
          const size_t oi = rawObj.getElementIndex();
          if(!args.objResolver.tryGetOrSwapAnyRows(rawObj, px, py, rx, ry)) {
            continue;
          }
          ObjInfo info {
            *obj,
            //Assume position always succeeds to evaluate
            TableAdapters::read(oi, *px, *py),
            //Rotation is optional
            rx && ry ? TableAdapters::read(oi, *rx, *ry) : glm::vec2{ 1.0f, 0.0f }
          };
          visitor.obj = &info;

          //TODO: visitor would be more efficient if it did the looping to avoid switching on each object
          std::visit(visitor, query.shape);
        }
      }
    }
  }

  //View the collision pairs output by the broadphase via updateCollisionPairs, add/remove pairs from narrowphase information for the queries,
  //then resolve all queries to produce updated results
  void physicsProcessQueries(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("SQ physics queries");
    SweepNPruneBroadphase::ChangedCollisionPairs* pairs = task.query<SharedRow<SweepNPruneBroadphase::ChangedCollisionPairs>>().tryGetSingletonElement();
    auto ids = task.getIDResolver();
    auto queries = task.query<
      const Physics<QueryRow>,
      Physics<ResultRow>,
      const StableIDRow
    >();
    using namespace Tags;
    auto objResolver = task.getResolver<
      const FloatRow<GPos, X>,
      const FloatRow<GPos, Y>,
      const FloatRow<GRot, CosAngle>,
      const FloatRow<GRot, SinAngle>
    >();
    task.setCallback([ids, queries, objResolver, pairs](AppTaskArgs&) mutable {
      for(size_t t = 0; t < queries.size(); ++t) {
        auto&& [physicsQuery, physicsResult, stableRow] = queries.get(t);
        PhysicsProcessArgs args {
          *ids,
          *objResolver,
          *physicsQuery,
          *physicsResult,
          *stableRow
        };

        //Each step depends on nearbyObjects so they're all sequential
        physicsProcessLosses(args, pairs->lostQueries);
        physicsProcessGains(args, pairs->gainedQueries);
        physicsProcessUpdates(args);

        pairs->lostQueries.clear();
        pairs->gainedQueries.clear();
      }
    });
    builder.submitTask(std::move(task));
  }

  void processLifetime(IAppBuilder& builder, const UnpackedDatabaseElementID& table) {
    auto task = builder.createTask();
    task.setName("SQ lifetimes");
    Globals* globals = task.query<Gameplay<GlobalsRow>>(table).tryGetSingletonElement();
    auto lifetimes = task.query<LifetimeRow, const StableIDRow>(table);
    assert(globals && lifetimes.size());
    task.setCallback([globals, lifetimes](AppTaskArgs&) mutable {
      for(size_t i = 0; i < lifetimes.get<0>().size(); ++i) {
        size_t& lifetime = lifetimes.get<0>(0).at(i);
        if(!lifetime) {
          const StableElementID id = StableElementID::fromStableRow(i, lifetimes.get<1>(0));
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

  void submitQueryUpdates(IAppBuilder& builder, const UnpackedDatabaseElementID& table) {
    auto task = builder.createTask();
    task.setName("Spatial query updates");
    auto query = task.query<
      const Gameplay<QueryRow>,
      Physics<QueryRow>,
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

  struct CommandVisitor {
    void operator()(const Command::NewQuery& command) {
      const size_t index = gameplayQueries.size();
      modifier.resizeWithIDs(index + 1, &command.id);
      //Add to physics and gameplay locations now
      //Using the NeedsResubmitRow not feasible here because it's after that has already been processed
      gameplayQueries.at(index) = command.query;
      physicsQueries.at(index) = command.query;
      gameplayLifetimes.at(index) = command.lifetime;
      publishNewElement(command.id);
    }

    void operator()(const Command::DeleteQuery& command) {
      //Publish the removal event which will cause removal from the broadphase and removal from the table
      publishRemovedElement(command.id);
    }

    ITableModifier& modifier;
    Gameplay<QueryRow>& gameplayQueries;
    Gameplay<LifetimeRow>& gameplayLifetimes;
    Physics<QueryRow>& physicsQueries;

    Events::CreatePublisher& publishNewElement;
    Events::DestroyPublisher& publishRemovedElement;
  };

  void processCommandBuffer(IAppBuilder& builder, const UnpackedDatabaseElementID& table) {
    auto task = builder.createTask();
    task.setName("SQ commands");
    auto modifier = task.getModifierForTable(table);
    auto query = task.query<
      Gameplay<GlobalsRow>,
      Gameplay<QueryRow>,
      Gameplay<LifetimeRow>,
      Physics<QueryRow>
    >(table);

    task.setCallback([query, modifier](AppTaskArgs& args)  mutable {
      Events::CreatePublisher create({ &args });
      Events::DestroyPublisher destroy({ &args });
      CommandVisitor visitor {
        *modifier,
        *query.getSingleton<1>(),
        *query.getSingleton<2>(),
        *query.getSingleton<3>(),
        create,
        destroy
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
      const UnpackedDatabaseElementID& table = tables.matchingTableIDs[i];
      CommonTasks::tryCopyRowSameSize<Physics<ResultRow>, Gameplay<ResultRow>>(builder, table, table);
      processLifetime(builder, table);
      submitQueryUpdates(builder, table);
      processCommandBuffer(builder, table);
    }
  }
}