#include "Precompile.h"

#include "AppBuilder.h"
#include "Narrowphase.h"
#include "SpatialPairsStorage.h"
#include "Physics.h"
#include "NotIspc.h"
#include "glm/glm.hpp"
#include "Geometric.h"
#include "glm/gtc/matrix_inverse.hpp"

namespace TableExt {
  inline glm::vec2 read(size_t i, const Row<float>& a, const Row<float>& b) {
    return { a.at(i), b.at(i) };
  }
};

namespace Narrowphase {
  //TODO: need to be able to know how much contact info is desired

  namespace Shape {
    //Not the actual center, more like the reference point
    struct CenterVisitor {
      glm::vec2 operator()(const Rectangle& v) const {
        return v.center;
      }
      glm::vec2 operator()(const Raycast& v) const {
        return v.start;
      }
      glm::vec2 operator()(const AABB& v) const {
        return v.min;
      }
      glm::vec2 operator()(const Circle& v) const {
        return v.pos;
      }
      glm::vec2 operator()(const std::monostate&) const {
        return { 0, 0 };
      }
    };

    glm::vec2 getCenter(const Variant& shape) {
      return std::visit(CenterVisitor{}, shape);
    }
  }


  struct ContactArgs {
    SP::ContactManifold& manifold;
    SP::ZContactManifold& zManifold;
  };

  //Non-implemented fallback
  template<class A, class B>
  void generateContacts(A&, B&, ContactArgs&) {
  }

  void swapAB(ContactArgs& result) {
    for(uint32_t i = 0; i < result.manifold.size; ++i) {
      auto& p = result.manifold.points[i];
      std::swap(p.centerToContactA, p.centerToContactB);
      p.normal = -p.normal;
    }
  }

  template<class A, class B>
  void generateSwappedContacts(const A& a, const B& b, ContactArgs& result) {
    generateContacts(b, a, result);
    swapAB(result);
  }

  void generateContacts(Shape::Rectangle& a, Shape::Rectangle& b, ContactArgs& result) {
    ispc::UniformConstVec2 pa{ &a.center.x, &a.center.y };
    ispc::UniformRotation ra{ &a.right.x, &a.right.y };
    ispc::UniformConstVec2 sa{ &a.halfWidth.x, &a.halfWidth.y };
    ispc::UniformConstVec2 pb{ &b.center.x, &b.center.y };
    ispc::UniformRotation rb{ &b.right.x, &b.right.y };
    ispc::UniformConstVec2 sb{ &b.halfWidth.x, &b.halfWidth.y };
    ispc::UniformVec2 normal{ &result.manifold.points[0].normal.x, &result.manifold.points[0].normal.y };
    glm::vec2 c1, c2;
    ispc::UniformContact uc1{ &c1.x, &c1.y, &result.manifold.points[0].overlap };
    ispc::UniformContact uc2{ &c2.x, &c2.y, &result.manifold.points[1].overlap };
    notispc::generateUnitCubeCubeContacts(pa, ra, sa, pb, rb, sb, normal, uc1, uc2, 1);
    if(result.manifold.points[0].overlap >= 0) {
      ++result.manifold.size;
      result.manifold.points[0].centerToContactA = c1 - a.center;
      result.manifold.points[0].centerToContactB = c1 - b.center;
    }
    if(result.manifold.points[1].overlap >= 0) {
      ++result.manifold.size;
      result.manifold.points[1].centerToContactA = c2 - a.center;
      result.manifold.points[1].centerToContactB = c2 - b.center;
      //Duplicate shared normal
      result.manifold.points[1].normal = result.manifold.points[0].normal;
    }
  }

  void generateContacts(Shape::Rectangle& cube, Shape::Circle& circle, ContactArgs& result) {
    const glm::vec2 toCircle = circle.pos - cube.center;
    const glm::vec2 up = Geo::orthogonal(cube.right);
    const float extentX = glm::dot(cube.right, toCircle);
    const float extentY = glm::dot(up, toCircle);
    const float SCALEX = cube.halfWidth.x;
    const float SCALEY = cube.halfWidth.y;
    const glm::vec2 closestOnSquare = glm::clamp(extentX, -SCALEX, SCALEX)*cube.right + glm::clamp(extentY, -SCALEY, SCALEY)*up;
    const glm::vec2 closestOnSquareToCircle = toCircle - closestOnSquare;
    const float dist2 = glm::dot(closestOnSquareToCircle, closestOnSquareToCircle);
    if(dist2 <= circle.radius) {
      result.manifold.size = 1;
      result.manifold[0].centerToContactA = closestOnSquare;
      //TODO:
      result.manifold[0].centerToContactB = glm::vec2{ 0 };
      result.manifold[0].normal = glm::vec2{ 0 };
    }
  }

  void generateContacts(Shape::Rectangle& cube, Shape::Raycast& ray, ContactArgs& result) {
    const glm::mat3 transform = Geo::buildTransform(cube.center, cube.right, cube.halfWidth * 2.0f);
    const glm::mat3 invTransform = glm::affineInverse(transform);
    const glm::vec2 localStart = Geo::transformPoint(invTransform, ray.start);
    const glm::vec2 localEnd = Geo::transformPoint(invTransform, ray.end);
    const glm::vec2 dir = localEnd - localStart;
    float tmin, tmax;
    if(Geo::unitAABBLineIntersect(localStart, dir, &tmin, &tmax)) {
      const glm::vec2 localPoint = localStart + dir*tmin;
      const glm::vec2 localNormal = Geo::getNormalFromUnitAABBIntersect(localPoint);
      result.manifold.size = 1;
      const glm::vec2 worldPoint = Geo::transformPoint(transform, localPoint);
      result.manifold[0].centerToContactA = worldPoint - cube.center;
      result.manifold[0].centerToContactB = worldPoint - ray.start;
      result.manifold[0].normal = -Geo::transformVector(transform, localNormal);
      //tmax - tmin is the part of the line that is intersecting with the shape. That projected onto the normal is the overlap
      result.manifold[0].overlap = std::abs(glm::dot((ray.end - ray.start)*(tmax - tmin), result.manifold[0].normal));
    }
  }

  void generateContacts(Shape::Raycast& ray, Shape::Rectangle& cube, ContactArgs& result) {
    generateSwappedContacts(ray, cube, result);
  }

  void generateContacts(Shape::Circle& circle, Shape::Rectangle& cube, ContactArgs& result) {
    generateSwappedContacts(circle, cube, result);
  }

  void generateContacts(const Shape::Rectangle&, const Shape::AABB&, ContactArgs&) {
    //TODO: generalize unit cube so it can be used for this
  }

  void generateContacts(const Shape::AABB& a, const Shape::Rectangle& b, ContactArgs& result) {
    generateSwappedContacts(a, b, result);
  }

  void generateContacts(const Shape::AABB&, const Shape::AABB&, ContactArgs&) {
    //TODO: for spatial queries is this needed?
  }

  void generateContacts(Shape::Circle& a, Shape::Circle& b, ContactArgs& result) {
    const glm::vec2 ab = b.pos - a.pos;
    const float dist2 = glm::dot(ab, ab);
    const float radii = a.radius + b.radius;
    constexpr float epsilon = 0.0001f;
    if(dist2 <= radii*radii) {
      auto& res = result.manifold.points[0];
      result.manifold.size = 1;

      float dist = std::sqrt(dist2);
      //Circles not on top of each-other, normal is vector between them
      if(dist > epsilon) {
        res.normal = -ab / dist;
      }
      else {
        res.normal = { 1, 0 };
      }
      res.centerToContactA = -res.normal*a.radius;
      res.centerToContactB = (a.pos + res.centerToContactA) - b.pos;
      res.overlap = radii - dist;
    }
  }

  void generateContacts(Shape::BodyType& a, Shape::BodyType& b, ContactArgs& result) {
    std::visit([&](auto& unwrappedA) {
      std::visit([&](auto& unwrappedB) {
        generateContacts(unwrappedA, unwrappedB, result);
      }, b.shape);
    }, a.shape);
  }

  struct SharedUnitCubeQueries {
    std::shared_ptr<ITableResolver> resolver;
    CachedRow<const SharedRectangleRow> definition;
    CachedRow<const Row<float>> centerX;
    CachedRow<const Row<float>> centerY;
    CachedRow<const Row<float>> rotX;
    CachedRow<const Row<float>> rotY;
    CachedRow<const Row<float>> scaleX;
    CachedRow<const Row<float>> scaleY;
    RectDefinition alias;
  };

  struct ShapeQueries {
    std::shared_ptr<ITableResolver> shapeResolver;
    std::shared_ptr<ITableResolver> zResolver;
    CachedRow<const RectangleRow> rectRow;
    CachedRow<const AABBRow> aabbRow;
    CachedRow<const RaycastRow> rayRow;
    CachedRow<const CircleRow> circleRow;
    CachedRow<const CollisionMaskRow> collisionMasksRow;
    CachedRow<const ThicknessRow> thicknessRow;
    CachedRow<const SharedThicknessRow> sharedThickness;
    SharedUnitCubeQueries sharedUnitCube;
    ConstFloatQueryAlias posZAlias;
    CachedRow<const Row<float>> posZ;
  };

  Geo::Range1D getZRange(const UnpackedDatabaseElementID& e, ShapeQueries& queries) {
    float z = Physics::DEFAULT_Z;
    if(const float* pz = queries.zResolver->tryGetOrSwapRowAliasElement(queries.posZAlias, queries.posZ, e)) {
      z = *pz;
    }
    float thickness = DEFAULT_THICKNESS;
    if(const float* shared = queries.shapeResolver->tryGetOrSwapRowElement(queries.sharedThickness, e)) {
      thickness = *shared;
    }
    else if(const float* individual = queries.shapeResolver->tryGetOrSwapRowElement(queries.thicknessRow, e)) {
      thickness = *individual;
    }
    return { z, z + thickness };
  }

  void tryCheckZ(const UnpackedDatabaseElementID& a, const UnpackedDatabaseElementID& b, ShapeQueries& queries, ContactArgs& result) {
    //If it's already not colliding on XY then Z doesn't matter
    if(!result.manifold.size) {
      return;
    }
    const Geo::Range1D rangeA = getZRange(a, queries);
    const Geo::Range1D rangeB = getZRange(b, queries);
    //If they are overlapping, solve on XZ only. If not overlapping, solve Z to ensure they don't pass through each-other on the Z axis
    const Geo::RangeOverlap overlap = Geo::classifyRangeOverlap(rangeA, rangeB);
    const float distance = Geo::getRangeDistance(overlap, rangeA, rangeB);
    const float minThickness = std::min(rangeA.length(), rangeB.length());
    //If the snapes are mostly overlapping on the Z axis the distance will be -minThickness
    //In that case, treat it as a XY collision and ignore Z
    //If the overlap is larger, solve Z
    constexpr float overlapTolerance = 0.01f;
    if(std::abs(distance + minThickness) > overlapTolerance) {
      result.zManifold.info = SP::ZInfo{
        Geo::getRangeNormal(overlap),
        distance,
      };
      result.manifold.clear();
    }
  }

  ShapeQueries buildShapeQueries(RuntimeDatabaseTaskBuilder& task, const RectDefinition& unitCube, const PhysicsAliases& aliases) {
    ShapeQueries result;
    result.shapeResolver = task.getResolver<
      const RectangleRow,
      const AABBRow,
      const RaycastRow,
      const CircleRow,
      const CollisionMaskRow,
      const ThicknessRow,
      const SharedThicknessRow
    >();
    result.sharedUnitCube.resolver = task.getAliasResolver(
      unitCube.centerX, unitCube.centerY,
      unitCube.rotX, unitCube.rotY,
      unitCube.scaleX, unitCube.scaleY
    );
    result.sharedUnitCube.alias = unitCube;
    result.zResolver = task.getAliasResolver(aliases.posZ);
    result.posZAlias = aliases.posZ.read();
    return result;
  }

  Shape::BodyType classifyShape(ShapeQueries& queries, const UnpackedDatabaseElementID& id) {
    //TODO: try most recent shape
    const size_t myIndex = id.getElementIndex();

    //Shared unit cube table
    if(queries.sharedUnitCube.resolver->tryGetOrSwapRow(queries.sharedUnitCube.definition, id)) {
      auto& q = queries.sharedUnitCube;
      const auto& qa = q.alias;
      if(q.resolver->tryGetOrSwapRowAlias(qa.centerX, q.centerX, id) &&
        q.resolver->tryGetOrSwapRowAlias(qa.centerY, q.centerY, id)
      ) {
        Shape::Rectangle rect;
        rect.center = TableExt::read(myIndex, *q.centerX, *q.centerY);
        //Rotation is optional
        if(q.resolver->tryGetOrSwapRowAlias(qa.rotX, q.rotX, id) &&
          q.resolver->tryGetOrSwapRowAlias(qa.rotY, q.rotY, id)
        ) {
          rect.right = TableExt::read(myIndex, *q.rotX, *q.rotY);
        }
        if(q.resolver->tryGetOrSwapRowAlias(qa.scaleX, q.scaleX, id) &&
          q.resolver->tryGetOrSwapRowAlias(qa.scaleY, q.scaleY, id)) {
          rect.halfWidth = TableExt::read(myIndex, *q.scaleX, *q.scaleY) * 0.5f;
        }
        return { Shape::Variant{ rect } };
      }
      //Cube with missing fields, empty shape
      return {};
    }

    //Individual unit cubes
    if(queries.shapeResolver->tryGetOrSwapRow(queries.rectRow, id)) {
      return { Shape::Variant{ queries.rectRow->at(myIndex) } };
    }

    //AABB
    if(queries.shapeResolver->tryGetOrSwapRow(queries.aabbRow, id)) {
      return { Shape::Variant{ queries.aabbRow->at(myIndex) } };
    }

    //Circle
    if(queries.shapeResolver->tryGetOrSwapRow(queries.circleRow, id)) {
      return { Shape::Variant{ queries.circleRow->at(myIndex) } };
    }

    if(const auto* ray = queries.shapeResolver->tryGetOrSwapRowElement(queries.rayRow, id)) {
      return { Shape::Variant{ *ray } };
    }

    return {};
  }

  uint8_t getCollisionMask(ShapeQueries& queries, const UnpackedDatabaseElementID& id) {
    return queries.shapeResolver->tryGetOrSwapRow(queries.collisionMasksRow, id) ?
      queries.collisionMasksRow->at(id.getElementIndex()) :
      uint8_t{};
  }

  //TODO: this should allow a way for a layer to avoid collisions with itself as is desirable for spatial queries
  bool shouldCompareShapes(ShapeQueries& queries, const UnpackedDatabaseElementID& a, const UnpackedDatabaseElementID& b) {
    return getCollisionMask(queries, a) & getCollisionMask(queries, b);
  }

  void generateInline(IAppBuilder& builder, const RectDefinition& unitCube, const PhysicsAliases& aliases) {
    auto task = builder.createTask();
    task.setName("generate contacts inline");
    auto shapeQuery = buildShapeQueries(task, unitCube, aliases);
    auto query = task.query<SP::ObjA, SP::ObjB, SP::ManifoldRow, SP::ZManifoldRow>();
    auto ids = task.getIDResolver();

    task.setCallback([shapeQuery, query, ids](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [a, b, manifold, zManifold] = query.get(t);
        for(size_t i = 0; i < a->size(); ++i) {
          StableElementID& stableA = a->at(i);
          StableElementID& stableB = b->at(i);
          //TODO: fast path if it's the same table as last time
          auto resolvedA = ids->tryResolveAndUnpack(stableA);
          auto resolvedB = ids->tryResolveAndUnpack(stableB);
          SP::ContactManifold& man = manifold->at(i);
          SP::ZContactManifold& zMan = zManifold->at(i);
          //Clear for the generation below to regenerate the results
          man.clear();
          zMan.clear();

          //If this happens presumably the element will get removed from the table momentarily
          if(!resolvedA || !resolvedB) {
            continue;
          }
          stableA = resolvedA->stable;
          stableB = resolvedB->stable;

          if(!shouldCompareShapes(shapeQuery, resolvedA->unpacked, resolvedB->unpacked)) {
            continue;
          }

          //TODO: is non-const because of ispc signature, should be const
          Shape::BodyType shapeA = classifyShape(shapeQuery, resolvedA->unpacked);
          Shape::BodyType shapeB = classifyShape(shapeQuery, resolvedB->unpacked);
          ContactArgs args{ man, zMan };
          generateContacts(shapeA, shapeB, args);
          tryCheckZ(resolvedA->unpacked, resolvedB->unpacked, shapeQuery, args);
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void generateContactsFromSpatialPairs(IAppBuilder& builder, const RectDefinition& unitCube, const PhysicsAliases& aliases) {
    generateInline(builder, unitCube, aliases);
  }

  std::shared_ptr<IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, const RectDefinition& unitCube, const PhysicsAliases& aliases) {
    struct Impl : IShapeClassifier {
      Impl(ShapeQueries q)
        : queries{ std::move(q) } {
      }

      Shape::BodyType classifyShape(const UnpackedDatabaseElementID& id) override {
        return Narrowphase::classifyShape(queries, id);
      }

      ShapeQueries queries;
    };
    return std::make_shared<Impl>(Narrowphase::buildShapeQueries(task, unitCube, aliases));
  }
}