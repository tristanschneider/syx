#include "Precompile.h"
#include "shapes/Line.h"
#include "AppBuilder.h"

namespace Shapes {
  ShapeRegistry::Raycast lineFromTransform(const Transform::PackedTransform& transform) {
    return { transform.pos2(), transform.transformPoint(glm::vec2{ 1, 0 }) };
  }

  Transform::PackedTransform toTransform(const ShapeRegistry::Raycast& v, float z) {
    Transform::PackedTransform t;
    // Transform that causes 1,0 to go to end and the translate to be start
    const glm::vec2 dir = v.end - v.start;
    const float len = std::max(0.01f, glm::length(dir));
    constexpr float minScale = 0.01f;
    if(len > 0) {
      Transform::Parts parts;
      parts.translate = glm::vec3{ v.start.x, v.start.y, z };
      parts.rot = dir / len;
      parts.scale = glm::vec2{ len, len };
      t = Transform::PackedTransform::build(parts);
    }
    else {
      //Hack to avoid uninvertible transform for zero length rays
      t.setPos(glm::vec3{ v.start.x, v.start.y, z });
      t.ax = minScale;
      t.by = minScale;
    }
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
