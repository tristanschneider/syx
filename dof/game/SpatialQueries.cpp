#include "Precompile.h"

#include "SpatialQueries.h"

#include "TableAdapters.h"
#include "Simulation.h"

#include "CommonTasks.h"
#include "DBEvents.h"

#include "glm/glm.hpp"

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

  const Result& getResult(size_t index, const SpatialQueryAdapter& adapter) {
    return adapter.results->at(index);
  }

  struct VisitBoundsArgs {
    size_t index;
    MinX& minX;
    MinY& minY;
    MaxX& maxX;
    MaxY& maxY;
  };

  void visitUpdateBoundary(const Raycast& shape, VisitBoundsArgs& args) {
    const glm::vec2 min = glm::min(shape.start, shape.end);
    const glm::vec2 max = glm::max(shape.start, shape.end);
    TableAdapters::write(args.index, min, args.minX, args.minY);
    TableAdapters::write(args.index, max, args.maxX, args.maxY);
  }

  void visitUpdateBoundary(const AABB& shape, VisitBoundsArgs& args) {
    TableAdapters::write(args.index, shape.min, args.minX, args.minY);
    TableAdapters::write(args.index, shape.max, args.maxX, args.maxY);
  }

  void visitUpdateBoundary(const Circle& shape, VisitBoundsArgs& args) {
    const glm::vec2 r{ shape.radius, shape.radius };
    TableAdapters::write(args.index, shape.pos - r, args.minX, args.minY);
    TableAdapters::write(args.index, shape.pos + r, args.maxX, args.maxY);
  }

  TaskRange physicsUpdateBoundaries(GameDB db) {
    auto root = TaskNode::create([db](...) {
      auto& table = std::get<SpatialQueriesTable>(db.db.mTables);
      VisitBoundsArgs args {
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
        args.index = i;
        std::visit([&](const auto& shape) { visitUpdateBoundary(shape, args); }, cmd.at(i).shape);
      }
    });
    return TaskBuilder::addEndSync(root);
  }

  TaskRange physicsProcessQueries(GameDB db) {
    TODO: where to get the pair information and how to get the geometry?
    Probably not worth copying like narrowphase constraints, so directly from object table
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

  struct VisitArgs {
    SpatialQueriesTable& queries;
    StableElementMappings& mappings;
    Events::Publisher& publishNewElement;
    Events::Publisher& publishRemovedElement;
  };

  void visitCommand(const Command::NewQuery& command, VisitArgs& args) {
    const size_t index = TableOperations::size(args.queries);
    TableOperations::stableResizeTable<GameDatabase>(args.queries, index + 1, args.mappings, &command.id);
    //Add to physics and gameplay locations now
    //Using the NeedsResubmitRow not feasible here because it's after that has already been processed
    std::get<Physics<QueryRow>>(args.queries.mRows).at(index) = command.query;
    std::get<Gameplay<QueryRow>>(args.queries.mRows).at(index) = command.query;
    std::get<Gameplay<LifetimeRow>>(args.queries.mRows).at(index) = command.lifetime;
    args.publishNewElement(command.id);
  }

  void visitCommand(const Command::DeleteQuery& command, VisitArgs& args) {
    //Publish the removal event which will cause removal from the broadphase and removal from the table
    args.publishRemovedElement(command.id);
  }

  std::shared_ptr<TaskNode> processCommandBuffer(SpatialQueriesTable& queries, StableElementMappings& mappings, Events::Publisher publishNewElement, Events::Publisher publishRemovedElement) {
    return TaskNode::create([&, publishNewElement, publishRemovedElement](...) mutable {
      Globals& globals = std::get<Gameplay<GlobalsRow>>(queries.mRows).at();
      VisitArgs args {
        queries,
        mappings,
        publishNewElement,
        publishRemovedElement
      };

      //Not necessary to lock because this task is synchronously scheduled
      for(size_t i = 0; i < globals.commandBuffer.size(); ++i) {
        std::visit([&](const auto& cmd) { visitCommand(cmd, args); }, globals.commandBuffer[i].data);
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
    root->mChildren.push_back(submitQueryUpdates(std::get<Physics<QueryRow>>(table.mRows), std::get<Gameplay<QueryRow>>(table.mRows), std::get<Gameplay<NeedsResubmitRow>>(table.mRows));
    //Sync for table additions and removals
    TaskBuilder::_addSyncDependency(*root, processCommandBuffer(table,
      TableAdapters::getStableMappings(db),
      Events::createPublisher(&Events::onNewElement, db),
      Events::createPublisher(&Events::onRemovedElement, db)));

    return TaskBuilder::addEndSync(root);
  }
}