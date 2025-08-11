#include "Precompile.h"
#include "shapes/AABB.h"
#include "AppBuilder.h"


namespace Shapes {
  ShapeRegistry::AABB aabbFromTransform(const Transform::PackedTransform& t) {
    //TODO: could have special case for if this isn't rotated, which should be always
    const Transform::Parts parts = t.decompose();
    return { Geo::toVec2(parts.translate), Geo::toVec2(parts.translate) + parts.scale };
  }

  const Transform::PackedTransform toTransform(const ShapeRegistry::AABB& bb, float z) {
    Transform::PackedTransform t;
    t.setPos(glm::vec3{ bb.min.x, bb.min.y, z });
    const glm::vec2 scale = bb.max - bb.min;
    t.ax = scale.x;
    t.by = scale.y;
    return t;
  }

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

      ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID&, const Transform::PackedTransform& transform, const Transform::PackedTransform&) final {
        return { aabbFromTransform(transform) };
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

      auto query = task.query<const Transform::WorldTransformRow, const AABBRow>(bounds.table);
      task.setCallback([query, &bounds](AppTaskArgs&) mutable {
        auto [transforms, bbs] = query.get(0);
        const size_t s = transforms->size();
        bounds.minX.resize(s);
        bounds.minY.resize(s);
        bounds.maxX.resize(s);
        bounds.maxY.resize(s);
        for(size_t i = 0; i < transforms->size(); ++i) {
          const Transform::PackedTransform& t = transforms->at(i);
          const ShapeRegistry::AABB bb = aabbFromTransform(t);
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
