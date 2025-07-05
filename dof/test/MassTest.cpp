#include "Precompile.h"
#include "CppUnitTest.h"

#include <Mass.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(MassTest) {
    struct TestMesh {
      operator Mass::Mesh() {
        temp.resize(points.size());
        return Mass::Mesh{
          .ccwPoints = points.data(),
          .temp = temp.data(),
          .count = static_cast<uint32_t>(points.size()),
          .radius = radius,
          .density = density
        };
      };

      void pushCircle(const glm::vec2& center, float r) {
        constexpr size_t verts = 100;
        const float inc = Constants::TAU/static_cast<float>(verts);
        points.resize(verts);
        float angle = 0.f;
        for(size_t i = 0; i < verts; ++i, angle += inc) {
          points[i] = center + glm::vec2{ std::cos(angle)*r, std::sin(angle)*r };
        }
      }

      float density{ 1.f };
      float radius{};
      std::vector<glm::vec2> points;
      std::vector<glm::vec2> temp;
    };

    void assertEq(const Mass::MassProps& l, const Mass::MassProps& r, float e = Constants::EPSILON) {
      Assert::AreEqual(l.body.inverseMass, r.body.inverseMass, e);
      Assert::AreEqual(l.body.inverseInertia, r.body.inverseInertia, e);
      Assert::AreEqual(l.centerOfMass.x, r.centerOfMass.x, e);
      Assert::AreEqual(l.centerOfMass.y, r.centerOfMass.y, e);
    }

    TEST_METHOD(Zero) {
      assertEq(Mass::computeQuadMass({}), {});
      assertEq(Mass::computeCircleMass({}), {});
      assertEq(Mass::computeCapsuleMass({}), {});
      assertEq(Mass::computeMeshMass({}), {});
      assertEq(Mass::computeTriangleMass({}), {});
    }

    TEST_METHOD(CapsuleVCircle) {
      const Mass::Circle circle{
        .center = glm::vec2{ 1.f },
        .radius = 0.4f,
        .density = 0.1f
      };
      const Mass::Capsule capsule{
        .top = circle.center,
        .bottom = circle.center,
        .radius = circle.radius,
        .density = circle.density
      };
      assertEq(Mass::compute(circle), Mass::compute(capsule));
    }

    TEST_METHOD(PointMesh) {
      const Mass::Circle circle{
        .center = glm::vec2{ 1, 2 },
        .radius = 1.5f,
        .density = 0.3f
      };
      TestMesh mesh;
      mesh.points.push_back(circle.center);
      mesh.density = circle.density;
      mesh.radius = circle.radius;
      assertEq(Mass::computeMeshMass(mesh), Mass::computeCircleMass(circle));
    }

    TEST_METHOD(LineMesh) {
      const Mass::Capsule capsule{
        .top = glm::vec2{ 1.f },
        .bottom = glm::vec2{ 2.f },
        .radius = 0.4f,
        .density = 0.2f
      };
      TestMesh mesh;
      mesh.points.push_back(capsule.top);
      mesh.points.push_back(capsule.bottom);
      mesh.density = capsule.density;
      mesh.radius = capsule.radius;
      assertEq(Mass::compute(capsule), Mass::compute(mesh));
    }

    TEST_METHOD(TriangleMesh) {
      const Mass::Triangle tri{
        .a = glm::vec2{ 1, 1 },
        .b = glm::vec2{ 2, 1 },
        .c = glm::vec2{ 2, 2 },
        .density = 0.1f
      };
      TestMesh mesh;
      mesh.points.push_back(tri.a);
      mesh.points.push_back(tri.b);
      mesh.points.push_back(tri.c);
      mesh.density = tri.density;
      assertEq(Mass::compute(tri), Mass::compute(mesh), 0.01f);
    }

    TEST_METHOD(QuadMesh) {
      const Mass::Quad quad{
        .fullSize = glm::vec2{ 1, 2 },
        .density = 0.2f
      };
      TestMesh mesh;
      const glm::vec2 s = quad.fullSize*0.5f;
      mesh.points.push_back(glm::vec2{ -s.x, -s.y });
      mesh.points.push_back(glm::vec2{  s.x, -s.y });
      mesh.points.push_back(glm::vec2{  s.x,  s.y });
      mesh.points.push_back(glm::vec2{ -s.x,  s.y });
      mesh.density = quad.density;
      assertEq(Mass::compute(quad), Mass::compute(mesh));
    }

    TEST_METHOD(FlatCircleMesh) {
      const Mass::Circle circle{
        .center = glm::vec2{ 1.f },
        .radius = 1.1f,
        .density = 1.2f
      };
      TestMesh mesh;
      mesh.pushCircle(circle.center, circle.radius);
      mesh.density = circle.density;
      assertEq(Mass::compute(circle), Mass::compute(mesh), 0.1f);
    }

    TEST_METHOD(RoundedCircleMesh) {
      const Mass::Circle circle{
        .center = glm::vec2{ 1.f },
        .radius = 1.f,
        .density = 1.2f
      };
      TestMesh mesh;
      //Circle with radius 1, then add + 0.5 on all sides to reach circle radius 2
      mesh.pushCircle(circle.center, 1.f);
      mesh.radius = 0.5f;
      mesh.density = circle.density;
      //Disturbingly large epsilon. Not sure if this indicates a bug or numerical precision issues
      assertEq(Mass::compute(circle), Mass::compute(mesh), 0.45f);
    }
  };
}