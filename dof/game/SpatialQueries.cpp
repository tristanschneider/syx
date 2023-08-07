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
  StableElementID createQuery(SpatialQueryAdapter& table, Query&& query, size_t lifetime) {
    //Immediately reserve the key, the entry will be created later
    StableElementID result = StableElementID::fromStableID(table.stableMappings->createKey());
    {
      //Enqueue the command for the end of the frame.
      //The lock is for multiple tasks submitting requests at the same time which does not
      //overlap with the synchronous processing of commands at the end of the frame
      SpatialQuery::Globals& globals = table.globals->at();
      std::lock_guard<std::mutex> lock{ globals.mutex };
      globals.commandBuffer.push_back({ SpatialQuery::Command::NewQuery{ result, std::move(query), lifetime } });
    }
    return result;
  }

  std::optional<size_t> getIndex(StableElementID& id, SpatialQueryAdapter& table) {
    if(std::optional<StableElementID> resolved = StableOperations::tryResolveStableIDWithinTable<GameDatabase>(id, *table.stable, *table.stableMappings)) {
      //Write down for later
      id = *resolved;
      //Return raw index that can be used directly into rows in the table
      return GameDatabase::ElementID{ resolved->mUnstableIndex }.getElementIndex();
    }
    return {};
  }

  void refreshQuery(size_t index, SpatialQueryAdapter& adapter, Query&& query, size_t newLifetime) {
    adapter.queries->at(index) = std::move(query);
    adapter.needsResubmit->at(index) = true;
    refreshQuery(index, adapter, newLifetime);
  }

  void refreshQuery(size_t index, SpatialQueryAdapter& adapter, size_t newLifetime) {
    //Lifetime updates by themselves don't need to set the resubmit flag because all of these are checked
    //anyway during the lifetime update
    adapter.lifetime->at(index) = newLifetime;
  }

  void refreshQuery(StableElementID& index, SpatialQueryAdapter& adapter, Query&& query, size_t newLifetime) {
    if(auto i = getIndex(index, adapter)) {
      refreshQuery(*i, adapter, std::move(query), newLifetime);
    }
  }

  void refreshQuery(StableElementID& index, SpatialQueryAdapter& adapter, size_t newLifetime) {
    if(auto i = getIndex(index, adapter)) {
      refreshQuery(*i, adapter, newLifetime);
    }
  }

  const Result& getResult(size_t index, const SpatialQueryAdapter& adapter) {
    return adapter.results->at(index);
  }

  const Result* tryGetResult(StableElementID& id, SpatialQueryAdapter& adapter) {
    auto i = getIndex(id, adapter);
    return i ? &getResult(*i, adapter) : nullptr;
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

  TaskRange physicsUpdateBoundaries(GameDB db) {
    auto root = TaskNode::create([db](...) {
      auto& table = std::get<SpatialQueriesTable>(db.db.mTables);
      BoundaryVisitor visitor {
        size_t{ 0 },
        std::get<Physics<MinX>>(table.mRows),
        std::get<Physics<MinY>>(table.mRows),
        std::get<Physics<MaxX>>(table.mRows),
        std::get<Physics<MaxY>>(table.mRows)
      };

      auto& cmd = std::get<Physics<QueryRow>>(table.mRows);
      //Currently goes over all of them. If the update flags were copied over those could be used to skip unchanged entries
      //Either way all of them will be written to the broadphase
      for(size_t i = 0; i < cmd.size(); ++i) {
        visitor.index = i;
        std::visit(visitor, cmd.at(i).shape);
      }
    });
    return TaskBuilder::addEndSync(root);
  }

  struct ObjInfo {
    StableElementID id;
    glm::vec2 pos{};
    glm::vec2 rot{};
  };

  ObjInfo readObjectInfo(const StableElementID& id, GameObjectAdapter& obj) {
    const size_t i = id.toPacked<GameDatabase>().getElementIndex();
    return {
      id,
      TableAdapters::read(i, *obj.transform.posX, *obj.transform.posY),
      //Default to no rotation if row is missing
      obj.transform.rotX ? TableAdapters::read(i, *obj.transform.rotX, *obj.transform.rotY) : glm::vec2(1.0f, 0.0f)
    };
  }

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

  //Copies the new pairs into the query `nearbyObjects` list which is then used in physicsProcessUpdates to see if they actually match
  void physicsProcessGains(GameDB db, const std::vector<SweepNPruneBroadphase::SpatialQueryPair>& pairs) {
    auto& table = std::get<SpatialQueriesTable>(db.db.mTables);
    auto& results = std::get<Physics<ResultRow>>(table.mRows);
    auto& mappings = TableAdapters::getStableMappings(db);
    auto& stableRow = std::get<StableIDRow>(table.mRows);
    for(const auto& pair : pairs) {
      if(auto q = StableOperations::tryResolveStableIDWithinTable<GameDatabase>(pair.query, stableRow, mappings)) {
        const GameDatabase::ElementID rawQuery = q->toPacked<GameDatabase>();
        results.at(rawQuery.getElementIndex()).nearbyObjects.push_back(pair.object);
      }
    }
  }

  //Removes from the `nearbyObjects` list which will prevent these from being put in the results list during physicsProcessUpdates
  void physicsProcessLosses(GameDB db, const std::vector<SweepNPruneBroadphase::SpatialQueryPair>& pairs) {
    auto& table = std::get<SpatialQueriesTable>(db.db.mTables);
    auto& results = std::get<Physics<ResultRow>>(table.mRows);
    auto& mappings = TableAdapters::getStableMappings(db);
    auto& stableRow = std::get<StableIDRow>(table.mRows);
    for(const auto& pair : pairs) {
      if(auto q = StableOperations::tryResolveStableIDWithinTable<GameDatabase>(pair.query, stableRow, mappings)) {
        const GameDatabase::ElementID rawQuery = q->toPacked<GameDatabase>();
        std::vector<StableElementID>& nearby = results.at(rawQuery.getElementIndex()).nearbyObjects;
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
  void physicsProcessUpdates(GameDB db) {
    auto& table = std::get<SpatialQueriesTable>(db.db.mTables);
    auto& queries = std::get<Physics<QueryRow>>(table.mRows);
    auto& results = std::get<Physics<ResultRow>>(table.mRows);
    auto& mappings = TableAdapters::getStableMappings(db);
    auto clearResults = [](auto& r) { r.results.clear(); };
    for(size_t i = 0; i < results.size(); ++i) {
      Result& r = results.at(i);
      //Clear the results completely and repopulate in the list below
      std::visit(clearResults, r.result);

      Query& query = queries.at(i);
      VisitPhysicsGain visitor;
      visitor.results = &r;

      //Check all nearby to see if they're still colliding
      //If so they will refill the results list
      //If not, nothing happens, as the list was cleared above so they won't be in the results
      for(size_t o = 0; o < r.nearbyObjects.size(); ++o) {
        StableElementID& nearby = r.nearbyObjects[o];
        if(const auto obj = StableOperations::tryResolveStableID(nearby, db.db, mappings)) {
          //Update mapping
          nearby = *obj;
          GameObjectAdapter object = TableAdapters::getObjectInTable(db, obj->toPacked<GameDatabase>().getTableIndex());
          ObjInfo info = readObjectInfo(*obj, object);
          visitor.obj = &info;

          //TODO: visitor would be more efficient if it did the looping to avoid switching on each object
          std::visit(visitor, query.shape);
        }
      }
    }
  }

  //View the collision pairs output by the broadphase via updateCollisionPairs, add/remove pairs from narrowphase information for the queries,
  //then resolve all queries to produce updated results
  TaskRange physicsProcessQueries(GameDB db) {
    auto& pairs = std::get<SharedRow<SweepNPruneBroadphase::ChangedCollisionPairs>>(std::get<BroadphaseTable>(db.db.mTables).mRows).at();
    auto root = TaskNode::create([&pairs, db](...) {
      //Each step depend on nearbyObjects so they're all sequential
      physicsProcessLosses(db, pairs.lostQueries);
      physicsProcessGains(db, pairs.gainedQueries);
      physicsProcessUpdates(db);

      pairs.lostQueries.clear();
      pairs.gainedQueries.clear();
    });
    return TaskBuilder::addEndSync(root);
  }

  std::shared_ptr<TaskNode> processLiftetime(Globals& globals, LifetimeRow& lifetimes, const StableIDRow& stable) {
    return TaskNode::create([&](...) {
      for(size_t i = 0; i < lifetimes.size(); ++i) {
        size_t& lifetime = lifetimes.at(i);
        if(!lifetime) {
          const StableElementID id = StableElementID::fromStableRow(i, stable);
          //Not necessary at the moment since it's synchronous but would be if this task was made to operate on ranges
          std::lock_guard<std::mutex> lock{ globals.mutex };
          globals.commandBuffer.push_back({ Command::DeleteQuery{ id } });
        }
        else {
          --lifetime;
        }
      }
    });
  }

  std::shared_ptr<TaskNode> submitQueryUpdates(Physics<QueryRow>& physics, const Gameplay<QueryRow>& gameplay, Gameplay<NeedsResubmitRow>& needsResubmit) {
    return TaskNode::create([&](...) {
      for(size_t i = 0; i < physics.size(); ++i) {
        //This assumes resubmits are frequent.
        //If not, it may be faster to add a command to point at the index of what needs to be resubmitted
        if(needsResubmit.at(i)) {
          needsResubmit.at(i) = false;
          physics.at(i) = gameplay.at(i);
        }
      }
    });
  }

  struct CommandVisitor {
    void operator()(const Command::NewQuery& command) {
      const size_t index = TableOperations::size(queries);
      TableOperations::stableResizeTable<GameDatabase>(queries, index + 1, mappings, &command.id);
      //Add to physics and gameplay locations now
      //Using the NeedsResubmitRow not feasible here because it's after that has already been processed
      std::get<Physics<QueryRow>>(queries.mRows).at(index) = command.query;
      std::get<Gameplay<QueryRow>>(queries.mRows).at(index) = command.query;
      std::get<Gameplay<LifetimeRow>>(queries.mRows).at(index) = command.lifetime;
      publishNewElement(command.id);
    }

    void operator()(const Command::DeleteQuery& command) {
      //Publish the removal event which will cause removal from the broadphase and removal from the table
      publishRemovedElement(command.id);
    }

    SpatialQueriesTable& queries;
    StableElementMappings& mappings;
    Events::Publisher& publishNewElement;
    Events::Publisher& publishRemovedElement;
  };

  std::shared_ptr<TaskNode> processCommandBuffer(SpatialQueriesTable& queries, StableElementMappings& mappings, Events::Publisher publishNewElement, Events::Publisher publishRemovedElement) {
    return TaskNode::create([&, publishNewElement, publishRemovedElement](...) mutable {
      Globals& globals = std::get<Gameplay<GlobalsRow>>(queries.mRows).at();
      CommandVisitor visitor {
        queries,
        mappings,
        publishNewElement,
        publishRemovedElement
      };

      //Not necessary to lock because this task is synchronously scheduled
      for(size_t i = 0; i < globals.commandBuffer.size(); ++i) {
        std::visit(visitor, globals.commandBuffer[i].data);
      }

      globals.commandBuffer.clear();
    });
  }

  TaskRange gameplayUpdateQueries(GameDB db) {
    auto root = TaskNode::createEmpty();
    SpatialQueryAdapter adapter = TableAdapters::getSpatialQueries(db);
    auto& table = std::get<SpatialQueriesTable>(db.db.mTables);

    //Update all separate rows in place in parallel
    root->mChildren.push_back(CommonTasks::copyRowSameSize<Physics<ResultRow>, Gameplay<ResultRow>>(table));
    root->mChildren.push_back(processLiftetime(adapter.globals->at(), *adapter.lifetime, *adapter.stable));
    root->mChildren.push_back(submitQueryUpdates(std::get<Physics<QueryRow>>(table.mRows), std::get<Gameplay<QueryRow>>(table.mRows), std::get<Gameplay<NeedsResubmitRow>>(table.mRows)));
    //Sync for table additions and removals
    TaskBuilder::_addSyncDependency(*root, processCommandBuffer(table,
      TableAdapters::getStableMappings(db),
      Events::createPublisher(&Events::onNewElement, db),
      Events::createPublisher(&Events::onRemovedElement, db)));

    return TaskBuilder::addEndSync(root);
  }
}