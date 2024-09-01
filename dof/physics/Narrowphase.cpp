#include "Precompile.h"

#include "AppBuilder.h"
#include "Narrowphase.h"
#include "SpatialPairsStorage.h"
#include "Physics.h"
#include "NotIspc.h"
#include "glm/glm.hpp"
#include "Geometric.h"
#include "glm/gtc/matrix_inverse.hpp"
#include "BoxBox.h"

namespace Narrowphase {
  //TODO: need to be able to know how much contact info is desired

  struct ContactArgs {
    SP::ContactManifold& manifold;
    SP::ZContactManifold& zManifold;
    SP::PairType& pairType;
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

  Narrowphase::BoxPairElement toBoxElement(const Shape::Rectangle& r) {
    return {
      r.center,
      r.right,
      r.halfWidth
    };
  }

  Narrowphase::BoxPairElement toBoxElement(const Shape::AABB& r) {
    const glm::vec2 halfSize = (r.max - r.min) * 0.5f;
    return {
      r.min + halfSize,
      glm::vec2{ 1.0f, 0.0f },
      halfSize
    };
  }

  void generateContacts(Shape::Rectangle& a, Shape::Rectangle& b, ContactArgs& result) {
    Narrowphase::BoxPair pair{ toBoxElement(a), toBoxElement(b) };
    Narrowphase::boxBox(result.manifold, pair);
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

  void generateContacts(Shape::Rectangle& a, Shape::AABB& b, ContactArgs& result) {
    Narrowphase::BoxPair pair{ toBoxElement(a), toBoxElement(b) };
    Narrowphase::boxBox(result.manifold, pair);
  }

  void generateContacts(Shape::AABB& a, Shape::Rectangle& b, ContactArgs& result) {
    generateSwappedContacts(a, b, result);
  }

  void generateContacts(Shape::AABB&, Shape::AABB&, ContactArgs&) {
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

  struct ShapeQueries {
    ShapeQueries createThreadLocalCopy() {
      ShapeQueries result;
      result.resolver = resolver;
      result.posZAlias = posZAlias;
      return result;
    }

    ITableResolver* resolver{};
    CachedRow<const CollisionMaskRow> collisionMasksRow;
    CachedRow<const ThicknessRow> thicknessRow;
    CachedRow<const SharedThicknessRow> sharedThickness;
    ConstFloatQueryAlias posZAlias;
    CachedRow<const Row<float>> posZ;
    std::shared_ptr<ITableResolver> ownedResolver;
  };

  Geo::Range1D getZRange(const UnpackedDatabaseElementID& e, ShapeQueries& queries) {
    float z = Physics::DEFAULT_Z;
    if(const float* pz = queries.resolver->tryGetOrSwapRowAliasElement(queries.posZAlias, queries.posZ, e)) {
      z = *pz;
    }
    float thickness = DEFAULT_THICKNESS;
    if(const float* shared = queries.resolver->tryGetOrSwapRowElement(queries.sharedThickness, e)) {
      thickness = *shared;
    }
    else if(const float* individual = queries.resolver->tryGetOrSwapRowElement(queries.thicknessRow, e)) {
      thickness = *individual;
    }
    return { z, z + thickness };
  }

  void tryCheckZ(const UnpackedDatabaseElementID& a, const UnpackedDatabaseElementID& b, ShapeQueries& queries, ContactArgs& result) {
    //If it's already not colliding on XY then Z doesn't matter
    if(!result.manifold.size) {
      //Default to XY if both are empty since Z can't indicate empty
      result.pairType = SP::PairType::ContactXY;
      return;
    }
    const Geo::Range1D rangeA = getZRange(a, queries);
    const Geo::Range1D rangeB = getZRange(b, queries);
    //If they are overlapping, solve on XZ only. If not overlapping, solve Z to ensure they don't pass through each-other on the Z axis
    const Geo::RangeOverlap overlap = Geo::classifyRangeOverlap(rangeA, rangeB);
    float distance = Geo::getRangeDistance(overlap, rangeA, rangeB);
    //If the shapes are within overlap tolerance, solve as XY collision
    //If they aren't, solve Z and ignore XY
    constexpr float overlapTolerance = Z_OVERLAP_TOLERANCE;
    if(distance > 0.0f) {
      result.zManifold.info = SP::ZInfo{
        Geo::getRangeNormal(overlap),
        //Provide information to solver as if they were closer to overlapping than they actually are
        //This keeps them at least twice the overlap tolerance apart, because if they got within the tolerance
        //then collision would not be prevented
        distance - overlapTolerance,
      };
      result.manifold.clear();
      result.pairType = SP::PairType::ContactZ;
    }
    else {
      result.pairType = SP::PairType::ContactXY;
    }
  }

  ShapeQueries buildShapeQueries(RuntimeDatabaseTaskBuilder& task, const PhysicsAliases& aliases) {
    ShapeQueries result;
    result.ownedResolver = task.getResolver<
      const CollisionMaskRow,
      const ThicknessRow,
      const SharedThicknessRow
    >();
    //Log dependency, use the resolver above
    task.getAliasResolver(aliases.posZ);
    result.posZAlias = aliases.posZ.read();

    result.resolver = result.ownedResolver.get();
    return result;
  }

  uint8_t getCollisionMask(ShapeQueries& queries, const UnpackedDatabaseElementID& id) {
    return queries.resolver->tryGetOrSwapRow(queries.collisionMasksRow, id) ?
      queries.collisionMasksRow->at(id.getElementIndex()) :
      uint8_t{};
  }

  //TODO: this should allow a way for a layer to avoid collisions with itself as is desirable for spatial queries
  bool shouldCompareShapes(ShapeQueries& queries, const UnpackedDatabaseElementID& a, const UnpackedDatabaseElementID& b) {
    return getCollisionMask(queries, a) & getCollisionMask(queries, b);
  }

  void generateInline(IAppBuilder& builder, const PhysicsAliases& aliases, size_t threadCount) {
    auto task = builder.createTask();
    std::vector<std::shared_ptr<ShapeRegistry::IShapeClassifier>> classifiers(threadCount, nullptr);
    const auto* reg = ShapeRegistry::get(task);
    for(size_t i = 0; i < threadCount; ++i) {
      classifiers[i] = reg->createShapeClassifier(task);
    }
    task.setName("generate contacts inline");

    auto config = task.getConfig();
    {
      auto setup = builder.createTask();
      auto q = setup.query<SP::ManifoldRow>();
      setup.setName("set task size");
      setup.setCallback([config, q](AppTaskArgs&) mutable {
        AppTaskSize size;
        size.batchSize = 10;
        size.workItemCount = 0;
        for(size_t t = 0; t < q.size(); ++t) {
          size.workItemCount += q.get<0>(t).size();
        }
        config->setSize(size);
      });
      builder.submitTask(std::move(setup));
    }

    auto sq = buildShapeQueries(task, aliases);
    auto query = task.query<const SP::ObjA, const SP::ObjB, SP::ManifoldRow, SP::ZManifoldRow, SP::PairTypeRow>();
    auto ids = task.getIDResolver();

    task.setCallback([sq, query, ids, classifiers](AppTaskArgs& args) mutable {
      ShapeQueries shapeQuery{ sq.createThreadLocalCopy() };
      ShapeRegistry::IShapeClassifier& classifier = *classifiers[args.threadIndex];
      auto resolver = ids->getRefResolver();
      size_t currentIndex = 0;
      for(size_t t = 0; t < query.size(); ++t) {
        auto [a, b, manifold, zManifold, pairType] = query.get(t);
        const size_t thisTableStart = currentIndex;
        const size_t thisTableEnd = thisTableStart + a->size();
        currentIndex += a->size();
        if(args.begin < thisTableStart) {
          break;
        }
        if(args.begin >= thisTableEnd) {
          continue;
        }
        for(size_t ri = args.begin; ri < std::min(args.end, thisTableEnd); ++ri) {
          const size_t i = ri - thisTableStart;
          const ElementRef& stableA = a->at(i);
          if(stableA == ElementRef{}) {
            continue;
          }
          SP::PairType& pairT = pairType->at(i);
          if(!SP::isContactPair(pairT)) {
            continue;
          }

          const ElementRef& stableB = b->at(i);
          //TODO: fast path if it's the same table as last time
          auto resolvedA = resolver.tryUnpack(stableA);
          auto resolvedB = resolver.tryUnpack(stableB);
          SP::ContactManifold& man = manifold->at(i);
          SP::ZContactManifold& zMan = zManifold->at(i);
          //Clear for the generation below to regenerate the results
          man.clear();
          zMan.clear();

          //If this happens presumably the element will get removed from the table momentarily
          if(!resolvedA || !resolvedB) {
            continue;
          }

          if(!shouldCompareShapes(shapeQuery, *resolvedA, *resolvedB)) {
            continue;
          }

          //TODO: is non-const because of ispc signature, should be const
          Shape::BodyType shapeA = classifier.classifyShape(*resolvedA);
          Shape::BodyType shapeB = classifier.classifyShape(*resolvedB);
          ContactArgs cargs{ man, zMan, pairT };
          generateContacts(shapeA, shapeB, cargs);
          tryCheckZ(*resolvedA, *resolvedB, shapeQuery, cargs);
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void generateContactsFromSpatialPairs(IAppBuilder& builder, const PhysicsAliases& aliases, size_t threadCount) {
    generateInline(builder, aliases, threadCount);
  }
}