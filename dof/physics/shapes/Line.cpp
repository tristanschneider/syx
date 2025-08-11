#include "Precompile.h"
#include "shapes/Line.h"
#include "AppBuilder.h"

namespace Shapes {
  ShapeRegistry::Raycast lineFromTransform(const Transform::PackedTransform& transform) {
    return { transform.pos2(), transform.pos2() + transform.basisX() };
  }

  Transform::PackedTransform toTransform(const ShapeRegistry::Raycast& v, float z) {
    Transform::PackedTransform t;
    t.setPos(glm::vec3{ v.start.x, v.start.y, z });
    const glm::vec2 scale = v.end - v.start;
    t.ax = scale.x;
    t.by = scale.y;
    return t;
  }

  struct IndividualLineShape : ShapeRegistry::IShapeImpl {
    std::vector<TableID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<const LineRow>().getMatchingTableIDs();
    }

    struct Classifier : ShapeRegistry::IShapeClassifier {
      Classifier(RuntimeDatabaseTaskBuilder&, ITableResolver&)
      {
      }

      ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID&, const Transform::PackedTransform& transform, const Transform::PackedTransform&) final {
        return { lineFromTransform(transform) };
      }
    };

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<Classifier>(task, resolver);
    }

    void writeBoundaries(IAppBuilder& builder, ShapeRegistry::BroadphaseBounds& bounds) const final {
      auto task = builder.createTask();
      task.setName("write line indiv bounds");
      task.logDependency({ bounds.requiredDependency });

      auto query = task.query<const Transform::WorldTransformRow, const LineRow>(bounds.table);
      task.setCallback([query, &bounds](AppTaskArgs&) mutable {
        auto [transforms, lines] = query.get(0);
        const size_t s = transforms->size();
        bounds.minX.resize(s);
        bounds.minY.resize(s);
        bounds.maxX.resize(s);
        bounds.maxY.resize(s);
        for(size_t i = 0; i < transforms->size(); ++i) {
          const ShapeRegistry::Raycast line = lineFromTransform(transforms->at(i));
          bounds.minX[i] = std::min(line.start.x, line.end.x);
          bounds.maxX[i] = std::max(line.start.x, line.end.x);
          bounds.minY[i] = std::min(line.start.y, line.end.y);
          bounds.maxY[i] = std::max(line.start.y, line.end.y);
        }
      });

      builder.submitTask(std::move(task));
    }
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualLine() {
    return std::make_unique<IndividualLineShape>();
  }
}
