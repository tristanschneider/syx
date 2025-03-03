#include "Precompile.h"
#include "FragmentSpawner.h"

#include "AppBuilder.h"
#include "IAppModule.h"
#include "RowTags.h"
#include "PhysicsSimulation.h"
#include "TransformResolver.h"
#include "Simulation.h"
#include <random>
#include "TLSTaskImpl.h"

#include "Events.h"
#include "GameDatabase.h"
#include "GraphicsTables.h"
#include "Narrowphase.h"

namespace FragmentSpawner {
  struct UpdateLocals {
    UpdateLocals(AppTaskArgs& args)
    {
      RuntimeDatabase& db = args.getLocalDB();
      const TableID table = db.query<FragmentSeekingGoalTagRow>().base().tryGet();
      fragmentTable = db.tryGet(table);
      if(!fragmentTable) {
        return;
      }
      fragmentTable->tryGet(posX)
        .tryGet(posY)
        .tryGet(posZ)
        .tryGet(goalX)
        .tryGet(goalY)
        .tryGet(sprite)
        .tryGet(stable);
    }

    explicit operator bool() const {
      return fragmentTable && posX && posY /*&& posZ*/ && goalX && goalY && sprite && stable;
    }

    RuntimeTable* fragmentTable{};
    Tags::PosXRow* posX{};
    Tags::PosYRow* posY{};
    Tags::PosZRow* posZ{};
    Tags::FragmentGoalXRow* goalX{};
    Tags::FragmentGoalYRow* goalY{};
    Row<CubeSprite>* sprite{};
    StableIDRow* stable{};
  };

  struct Update {
    Update(RuntimeDatabaseTaskBuilder& task)
      : q{ task}
      , transform{ PhysicsSimulation::createGameplayFullTransformResolver(task) }
    {
    }

    struct Grid {
      size_t rows{};
      size_t columns{};
      float fragmentScale{};
    };

    static Grid computeGridFromScale(const glm::vec2& scale, size_t fragmentCount) {
      const auto divOrOne = [](float n, float d) { return d ? n/d : 1.f; };
      const float aspectRatio = divOrOne(scale.y, scale.x);
      //Find (c, r) such that c*r = fragmentCount and r/c = y/x
      //c = fragmentCount/r
      //r/(fragmentCount/r) = y/x
      //r = sqrt(fragmentCount*y/x)
      const float fRows = std::sqrt(static_cast<float>(fragmentCount)*aspectRatio);
      const float fCols = divOrOne(static_cast<float>(fragmentCount), fRows);
      Grid result{
        .rows = static_cast<size_t>(std::ceil(fRows)),
        .columns = static_cast<size_t>(std::ceil(fCols))
      };
      //Since they are computed with aspect ratio computing scale based on X or Y should be equivalent
      result.fragmentScale = divOrOne(scale.x, static_cast<float>(result.rows));
      return result;
    }

    static Grid computeGridFromScale(const glm::vec2& scale) {
      return Grid{
        .rows = static_cast<size_t>(std::ceil(scale.x*2.f)),
        .columns = static_cast<size_t>(std::ceil(scale.y*2.f)),
        .fragmentScale = 1.f
      };
    }

    void spawnNewFragments(UpdateLocals& locals, const FragmentSpawnerCount&, const Narrowphase::CollisionMask collisionMask, const pt::FullTransform& spawnerTransform, AppTaskArgs& args) {
      collisionMask;
      //if(!config.fragmentCount) {
      //  return;
      //}
      args.getLocalDB().setTableDirty(locals.fragmentTable->getID());

      //const Grid grid = computeGridFromScale(spawnerTransform.scale, config.fragmentCount);
      const Grid grid = computeGridFromScale(spawnerTransform.scale);
      std::vector<size_t> indices(grid.rows*grid.columns);
      const size_t total = indices.size();
      size_t begin = locals.fragmentTable->addElements(indices.size());
      std::random_device device;
      std::mt19937 generator(device());

      int counter = 0;
      std::generate(indices.begin(), indices.end(), [&counter] { return counter++; });
      std::shuffle(indices.begin(), indices.end(), generator);
      //Make sure none start at their completed index
      for(size_t j = 0; j < total; ++j) {
        if(indices[j] == j) {
          std::swap(indices[j], indices[(j + 1) % total]);
        }
      }

      const float startX = -float(grid.columns)/2.0f;
      const float startY = -float(grid.rows)/2.0f;
      const float scaleX = 1.0f/float(grid.columns);
      const float scaleY = 1.0f/float(grid.rows);
      for(size_t j = 0; j < total; ++j) {
        const size_t shuffleIndex = indices[j];
        CubeSprite& sprite = locals.sprite->at(j);
        const size_t row = j / grid.columns;
        const size_t column = j % grid.columns;
        const size_t shuffleRow = shuffleIndex / grid.columns;
        const size_t shuffleColumn = shuffleIndex % grid.columns;
        //Goal position and uv is based on original index, starting position is based on shuffled index
        sprite.uMin = float(column)/float(grid.columns);
        sprite.vMin = float(row)/float(grid.rows);
        sprite.uMax = sprite.uMin + scaleX;
        sprite.vMax = sprite.vMin + scaleY;

        const size_t i = j + begin;
        locals.goalX->at(i) = startX + sprite.uMin*float(grid.columns);
        locals.goalY->at(i) = startY + sprite.vMin*float(grid.rows);

        //Flip from assuming bottom left texture origin to top left
        sprite.vMax = 1.f - sprite.vMax;
        sprite.vMin = 1.f - sprite.vMin;
        std::swap(sprite.vMax, sprite.vMin);

        locals.posX->at(i) = startX + shuffleColumn;
        locals.posY->at(i) = startY + shuffleRow;
        //locals.posZ->at(i) = spawnerTransform.pos.z;
      }
    }

    void execute(UpdateLocals& locals, AppTaskArgs& args) {
      if(!locals) {
        return;
      }

      for(size_t t = 0; t < q.size(); ++t) {
        auto [stables, states, configs, collisionMasks] = q.get(t);
        for(size_t e = 0; e < stables.size; ++e) {
          switch(states->at(e)) {
          case FragmentSpawnState::New:
            spawnNewFragments(locals, configs->at(e), collisionMasks->at(e), transform.resolve(stables->at(e)), args);
            states->at(e) = FragmentSpawnState::Spawned;
            break;
          case FragmentSpawnState::Spawned:
            break;
          }
        }
      }
    }

    QueryResult<
      const StableIDRow,
      FragmentSpawnStateRow,
      const FragmentSpawnerCountRow,
      const Narrowphase::CollisionMaskRow
    > q;
    pt::FullTransformResolver transform;
  };

  struct ForwardConfigToFragments {
    ForwardConfigToFragments(RuntimeDatabaseTaskBuilder& task)
      : tables{ task }
      , res{ task.getResolver(dstTextures, dstMeshes) }
      , query{ task }
    {
    }

    void execute(AppTaskArgs&) {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [events, srcTextures, srcMeshes, tag] = query.get(t);
        //If no events happened, there must be nothing to forward
        if(!events->size()) {
          continue;
        }
        //If there are no assets, there's nothing to forward
        if(!srcTextures->at().asset && !srcMeshes->at().asset) {
          continue;
        }

        for (TableID table : { tables.activeFragment, tables.completedFragment }) {
          if(res->tryGetOrSwapRow(dstTextures, table)) {
            dstTextures->at() = srcTextures->at();
          }
          if(res->tryGetOrSwapRow(dstMeshes, table)) {
            dstMeshes->at() = srcMeshes->at();
          }
        }

        //Once they have been forwarded, the originals are no longer necessary
        srcTextures->at() = {};
        srcMeshes->at() = {};
      }
    }

    GameDatabase::Tables tables;
    CachedRow<SharedTextureRow> dstTextures;
    CachedRow<SharedMeshRow> dstMeshes;
    std::shared_ptr<ITableResolver> res;
    QueryResult<const Events::EventsRow, SharedTextureRow, SharedMeshRow, const FragmentSpawnerTagRow> query;
  };

  struct Module : IAppModule {
    void update(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<Update, DefaultTaskGroup, UpdateLocals>("update fragment spawner"));
    }

    void preProcessEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<ForwardConfigToFragments>("fwdfragment"));
    }
  };

  std::unique_ptr<IAppModule> createModule() {
    return std::make_unique<Module>();
  }
}