#include "shapes/Rectangle.h"
#include "AppBuilder.h"
#include "Physics.h"
#include <math/Geometric.h>
#include <transform/TransformRows.h>

namespace TableExt {
  inline glm::vec2 read(size_t i, const Row<float>& a, const Row<float>& b) {
    return { a.at(i), b.at(i) };
  }
};

namespace Shapes {
  struct RectShape : ShapeRegistry::IShapeImpl {
    std::vector<TableID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<const RectangleRow>().getMatchingTableIDs();
    }

    struct Classifier : ShapeRegistry::IShapeClassifier {
      Classifier(RuntimeDatabaseTaskBuilder&, ITableResolver&)
      {
      }

      ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID&, const Transform::PackedTransform& transform, const Transform::PackedTransform&) final {
        const Transform::Parts parts = transform.decompose();
        return {
          ShapeRegistry::Rectangle{
            .center = parts.translate,
            .right = parts.rot,
            .halfWidth = parts.scale
          }
        };
      }
    };

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<Classifier>(task, resolver);
    }

    void writeBoundaries(IAppBuilder& builder, ShapeRegistry::BroadphaseBounds& bounds) const final {
      auto task = builder.createTask();
      task.setName("write rect indiv bounds");
      task.logDependency({ bounds.requiredDependency });

      auto query = task.query<const RectangleRow, const Transform::WorldTransformRow>(bounds.table);
      task.setCallback([query, &bounds](AppTaskArgs&) mutable {
        auto [_, transforms] = query.get(0);
        const size_t s = transforms->size();
        bounds.minX.resize(s);
        bounds.minY.resize(s);
        bounds.maxX.resize(s);
        bounds.maxY.resize(s);
        for(size_t i = 0; i < transforms->size(); ++i) {
          const Transform::PackedTransform& t = transforms->at(i);
          const glm::vec2 extents = glm::abs(t.basisX()) + glm::abs(t.basisY());
          const glm::vec2 center = t.pos2();
          bounds.minX[i] = center.x - extents.x;
          bounds.maxX[i] = center.x + extents.x;
          bounds.minY[i] = center.y - extents.y;
          bounds.maxY[i] = center.y + extents.y;
        }
      });

      builder.submitTask(std::move(task));
    }
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createRectangle() {
    return std::make_unique<RectShape>();
  }
}
