#include <Precompile.h>
#include <CppUnitTest.h>

#include <MeshNarrowphase.h>
#include <glm/vec2.hpp>
#include <shapes/ShapeRegistry.h>
#include <SpatialPairsStorage.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(MeshNarrowphaseTest) {
    struct TestMesh {
      operator ShapeRegistry::Mesh() const {
        return ShapeRegistry::Mesh{
          .points = points,
          .modelToWorld = transform,
          .worldToModel = transform.inverse()
        };
      }

      void pushQuad(const glm::vec2& center, const glm::vec2& halfSize) {
        points.insert(points.end(), {
          glm::vec2{ center.x - halfSize.x, center.y - halfSize.y },
          glm::vec2{ center.x + halfSize.x, center.y - halfSize.y },
          glm::vec2{ center.x + halfSize.x, center.y + halfSize.y },
          glm::vec2{ center.x - halfSize.x, center.y + halfSize.y }
        });
      }

      void pushRightTriangle(const glm::vec2& rightCorner, const glm::vec2& extents) {
        points.insert(points.end(), {
          rightCorner,
          glm::vec2{ rightCorner.x + extents.x, rightCorner.y },
          glm::vec2{ rightCorner.x, rightCorner.y + extents.y }
        });
      }

      void pushCircle(const glm::vec2& center, float radius) {
        constexpr size_t verts = 100;
        constexpr float inc = Constants::TAU/static_cast<float>(verts);
        float angle{};
        for(size_t i = 0; i < verts; ++i, angle += inc) {
          points.push_back(center + glm::vec2{ std::cos(angle) * radius, std::sin(angle) * radius });
        }
      }

      void setPos(const glm::vec2& p) {
        transform.setPos(p);
      }

      void setRot(float r) {
        auto parts = transform.decompose();
        parts.rot = glm::vec2{ std::cos(r), std::sin(r) };
        transform = pt::PackedTransform::build(parts);
      }

      void setScale(const glm::vec2& scale) {
        auto parts = transform.decompose();
        parts.scale = scale;
        transform = pt::PackedTransform::build(parts);
      }

      std::vector<glm::vec2> points;
      pt::PackedTransform transform;
    };

    struct TestPair {
      void generateContacts() {
        manifold.clear();
        Narrowphase::generateContactsConvex(a, b, {}, manifold);
      }

      void assertContactPoints(std::initializer_list<glm::vec2> points, bool ignoreNormal) {
        Assert::AreEqual(static_cast<uint32_t>(points.size()), manifold.size);
        for(const glm::vec2& p : points) {
          bool found = false;
          for(uint32_t i = 0; i < manifold.size; ++i) {
            const glm::vec2 mpA = manifold.points[i].centerToContactA + a.transform.pos2();
            const glm::vec2 mpB = mpA - manifold.points[i].normal*manifold.points[i].overlap;
            Assert::IsTrue(ignoreNormal || glm::dot(manifold.points[i].normal, manifold.points[i].centerToContactA) <= 0.f, L"Normal should point towards A");
            found = found || Geo::near(p, mpA) || Geo::near(p, mpB);
          }
          Assert::IsTrue(found);
        }
      }

      void generateContactsAndAssertPoints(std::initializer_list<glm::vec2> points, bool ignoreNormal = false) {
        generateContacts();
        assertContactPoints(points, ignoreNormal);
      }

      TestMesh a, b;
      SP::ContactManifold manifold;
    };

    static constexpr bool IGNORE_NORMAL = true;
    static constexpr float noCollisionSpace = 0.01f;

    TEST_METHOD(QuadQuad) {
      TestPair pair;
      const glm::vec2 s{ 0.5f, 0.5f };
      pair.a.pushQuad({}, s);
      pair.b.pushQuad({}, s);
      pair.b.setPos(glm::vec2{ s.x*2.f + noCollisionSpace, 0.f });

      //Try equivalent rotations
      for(int i = 0; i < 4; ++i) {
        pair.a.setRot(static_cast<float>(i)*Constants::TAU/4.f);
        pair.b.setRot(static_cast<float>(-i)*Constants::TAU/4.f);
        pair.generateContactsAndAssertPoints({});
      }

      //Try equivalent no-collision scale
      pair.a.setScale(glm::vec2{ 0.5f, 0.5f });
      pair.b.setScale(glm::vec2{ 1.5f, 1.5f });
      pair.generateContactsAndAssertPoints({});

      pair.a.setScale(glm::vec2{ 1.f });
      pair.b.setScale(glm::vec2{ 1.f });
      pair.b.setPos(glm::vec2{ s.x*2.f - noCollisionSpace, 0.f });

      for(int i = 0; i < 4; ++i) {
        pair.a.setRot(static_cast<float>(i)*Constants::TAU/4.f);
        pair.b.setRot(static_cast<float>(-i)*Constants::TAU/4.f);
        pair.generateContactsAndAssertPoints({
          glm::vec2{ s.x, s.y },
          glm::vec2{ s.x, -s.y }
        });
      }

      pair.a.setScale(glm::vec2{ 0.5f, 0.5f });
      pair.b.setScale(glm::vec2{ 1.5f, 1.5f });
      pair.generateContactsAndAssertPoints({
        glm::vec2{ s.x*0.5f, s.y*0.5f },
        glm::vec2{ s.x*0.5f, -s.y*0.5f }
      });
    }

    TEST_METHOD(TriTri) {
      TestPair pair;
      pair.a.pushRightTriangle({}, glm::vec2{ 1.f });
      pair.b.pushRightTriangle({}, glm::vec2{ 1.f });
      pair.b.setPos(glm::vec2{ -1.f + noCollisionSpace, 0.5f });

      pair.generateContactsAndAssertPoints({
        glm::vec2{ noCollisionSpace, 0.5f },
        //Height of intersecting right triangle 45 degrees whose base is of length `noCollisionSpace`
        glm::vec2{ 0.f, 0.5f + std::tan(Constants::PI/4.f)*noCollisionSpace }
      //Hack to ignore normal validation since it only makes sense if center of model is centroid which is not the case here
      }, IGNORE_NORMAL);

      //A on bottom left and B on top right with the hypotenuse facing and nearly touching, separated by noCollisionSpace
      pair.b.setPos(glm::vec2{ 1.f + noCollisionSpace });
      pair.b.setRot(Constants::PI);
      pair.generateContactsAndAssertPoints({});
    }

    TEST_METHOD(CircleCircle) {
      TestPair pair;
      pair.a.pushCircle({}, 1.f);
      pair.b.pushCircle({}, 1.f);
      const glm::vec2 r = glm::normalize(glm::vec2{ 1 });
      pair.b.setPos(r*(2.f + noCollisionSpace));

      pair.generateContactsAndAssertPoints({});

      pair.b.setPos(r*(2.f - noCollisionSpace));
      pair.generateContactsAndAssertPoints({
        glm::vec2{ 0.678175f, 0.722594f },
        glm::vec2{ 0.722595f, 0.678174f }
      });
    }
  };
}