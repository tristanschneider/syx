#include "Precompile.h"

#include "AppBuilder.h"
#include "Narrowphase.h"
#include "SpatialPairsStorage.h"
#include "Physics.h"
#include "NotIspc.h"
#include "glm/glm.hpp"

namespace TableExt {
  inline glm::vec2 read(size_t i, const Row<float>& a, const Row<float>& b) {
    return { a.at(i), b.at(i) };
  }
};

namespace Narrowphase {
  //TODO: need to be able to know how much contact info is desired

  struct ContactArgs {
    SP::ContactManifold& manifold;
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

  void generateContacts(Shape::UnitCube& a, Shape::UnitCube& b, ContactArgs& result) {
    ispc::UniformConstVec2 pa{ &a.center.x, &a.center.y };
    ispc::UniformRotation ra{ &a.right.x, &a.right.y };
    ispc::UniformConstVec2 pb{ &b.center.x, &b.center.y };
    ispc::UniformRotation rb{ &b.right.x, &b.right.y };
    ispc::UniformVec2 normal{ &result.manifold.points[0].normal.x, &result.manifold.points[0].normal.y };
    glm::vec2 c1, c2;
    ispc::UniformContact uc1{ &c1.x, &c1.y, &result.manifold.points[0].overlap };
    ispc::UniformContact uc2{ &c2.x, &c2.y, &result.manifold.points[1].overlap };
    notispc::generateUnitCubeCubeContacts(pa, ra, pb, rb, normal, uc1, uc2, 1);
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

  void generateContacts(const Shape::UnitCube&, const Shape::AABB&, ContactArgs&) {
    //TODO: generalize unit cube so it can be used for this
  }

  void generateContacts(const Shape::AABB& a, const Shape::UnitCube& b, ContactArgs& result) {
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
    CachedRow<const SharedUnitCubeRow> definition;
    CachedRow<const Row<float>> centerX;
    CachedRow<const Row<float>> centerY;
    CachedRow<const Row<float>> rotX;
    CachedRow<const Row<float>> rotY;
    UnitCubeDefinition alias;
  };

  struct ShapeQueries {
    std::shared_ptr<ITableResolver> shapeResolver;
    CachedRow<const UnitCubeRow> unitCubeRow;
    CachedRow<const AABBRow> aabbRow;
    CachedRow<const CircleRow> circleRow;
    CachedRow<const CollisionMaskRow> collisionMasksRow;
    SharedUnitCubeQueries sharedUnitCube;
  };

  ShapeQueries buildShapeQueries(RuntimeDatabaseTaskBuilder& task, const UnitCubeDefinition& unitCube) {
    ShapeQueries result;
    result.shapeResolver = task.getResolver<
      const UnitCubeRow,
      const AABBRow,
      const CircleRow,
      const CollisionMaskRow
    >();
    result.sharedUnitCube.resolver = task.getAliasResolver(
      unitCube.centerX, unitCube.centerY,
      unitCube.rotX, unitCube.rotY
    );
    result.sharedUnitCube.alias = unitCube;
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
        Shape::UnitCube cube;
        cube.center = TableExt::read(myIndex, *q.centerX, *q.centerY);
        //Rotation is optional
        if(q.resolver->tryGetOrSwapRowAlias(qa.rotX, q.rotX, id) &&
          q.resolver->tryGetOrSwapRowAlias(qa.rotY, q.rotY, id)
        ) {
          cube.right = TableExt::read(myIndex, *q.rotX, *q.rotY);
        }
        return { Shape::Variant{ cube } };
      }
      //Cube with missing fields, empty shape
      return {};
    }

    //Individual unit cubes
    if(queries.shapeResolver->tryGetOrSwapRow(queries.unitCubeRow, id)) {
      return { Shape::Variant{ queries.unitCubeRow->at(myIndex) } };
    }

    //AABB
    if(queries.shapeResolver->tryGetOrSwapRow(queries.aabbRow, id)) {
      return { Shape::Variant{ queries.aabbRow->at(myIndex) } };
    }

    //Circle
    if(queries.shapeResolver->tryGetOrSwapRow(queries.circleRow, id)) {
      return { Shape::Variant{ queries.circleRow->at(myIndex) } };
    }

    return {};
  }

  uint8_t getCollisionMask(ShapeQueries& queries, const UnpackedDatabaseElementID& id) {
    return queries.shapeResolver->tryGetOrSwapRow(queries.collisionMasksRow, id) ?
      queries.collisionMasksRow->at(id.getElementIndex()) :
      uint8_t{};
  }

  bool shouldCompareShapes(ShapeQueries& queries, const UnpackedDatabaseElementID& a, const UnpackedDatabaseElementID& b) {
    return getCollisionMask(queries, a) & getCollisionMask(queries, b);
  }

  void generateInline(IAppBuilder& builder, const UnitCubeDefinition& unitCube) {
    auto task = builder.createTask();
    task.setName("generate contacts inline");
    auto shapeQuery = buildShapeQueries(task, unitCube);
    auto query = task.query<SP::ObjA, SP::ObjB, SP::ManifoldRow>();
    auto ids = task.getIDResolver();

    task.setCallback([shapeQuery, query, ids](AppTaskArgs&) mutable {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [a, b, manifold] = query.get(t);
        for(size_t i = 0; i < a->size(); ++i) {
          StableElementID& stableA = a->at(i);
          StableElementID& stableB = b->at(i);
          //TODO: fast path if it's the same table as last time
          auto resolvedA = ids->tryResolveAndUnpack(stableA);
          auto resolvedB = ids->tryResolveAndUnpack(stableB);
          SP::ContactManifold& man = manifold->at(i);
          //Clear for the generation below to regenerate the results
          man.clear();

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
          ContactArgs args{ man };
          generateContacts(shapeA, shapeB, args);
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void generateContactsFromSpatialPairs(IAppBuilder& builder, const UnitCubeDefinition& unitCube) {
    generateInline(builder, unitCube);
  }
}