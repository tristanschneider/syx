#include "Precompile.h"
#include "shapes/AABB.h"
#include "AppBuilder.h"

namespace Shapes {
  struct IndividualAABBShape : ShapeRegistry::IShapeImpl {
    std::vector<TableID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<const AABBRow>().getMatchingTableIDs();
    }

    struct Classifier : ShapeRegistry::IShapeClassifier {
      Classifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& res)
        : resolver{ res }
      {
        //Log the dependency with get, but use the shared resolver
        task.getResolver(row);
      }

      ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID& id) final {
        if(const ShapeRegistry::AABB* bb = resolver.tryGetOrSwapRowElement(row, id)) {
          return { { *bb } };
        }
        return {};
      }

      ITableResolver& resolver;
      CachedRow<const AABBRow> row;
    };

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<Classifier>(task, resolver);
    }

    void writeBoundaries(IAppBuilder& builder, ShapeRegistry::BroadphaseBounds& bounds) const final {
      auto task = builder.createTask();
      task.setName("write aabb indiv bounds");
      task.logDependency({ bounds.requiredDependency });

      auto query = task.query<const AABBRow>(bounds.table);
      task.setCallback([query, &bounds](AppTaskArgs&) mutable {
        auto [bbs] = query.get(0);
        const size_t s = bbs->size();
        bounds.minX.resize(s);
        bounds.minY.resize(s);
        bounds.maxX.resize(s);
        bounds.maxY.resize(s);
        for(size_t i = 0; i < bbs->size(); ++i) {
          const ShapeRegistry::AABB& bb = bbs->at(i);
          bounds.minX[i] = bb.min.x;
          bounds.maxX[i] = bb.max.x;
          bounds.minY[i] = bb.min.y;
          bounds.maxY[i] = bb.max.y;
        }
      });

      builder.submitTask(std::move(task));
    }
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualAABB() {
    return std::make_unique<IndividualAABBShape>();
  }
}
