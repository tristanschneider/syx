#include <Precompile.h>
#include <CppUnitTest.h>

#include <transform/Transform.h>
#include <glm/gtx/transform.hpp>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(TransformTest) {
    static constexpr float E = 0.001f;

    void eq(const glm::vec3& l, const glm::vec3& r) {
      Assert::AreEqual(l.x, r.x, E);
      Assert::AreEqual(l.y, r.y, E);
      Assert::AreEqual(l.z, r.z, E);
    }

    void eq(const glm::vec3& l, const glm::vec2& r) {
      Assert::AreEqual(l.x, r.x, E);
      Assert::AreEqual(l.y, r.y, E);
    }

    TEST_METHOD(Transform_MatchesGLM) {
      const glm::vec3 pos{ 1, 2, 3 };
      const glm::vec3 scale{ 2, 4, 1 };
      const float rotRad = 0.1f;
      const glm::mat4 tm = glm::translate(pos);
      const glm::mat4 rm = glm::rotate(rotRad, glm::vec3{ 0, 0, 1 });
      const glm::mat4 sm = glm::scale(scale);
      const glm::mat4 glmTransform = tm*rm*sm;
      const Transform::PackedTransform transform = Transform::PackedTransform::build(
        Transform::Parts{
          .rot = glm::vec2{ std::cos(rotRad), std::sin(rotRad) },
          .scale = glm::vec2{ scale.x, scale.y },
          .translate = pos
        }
      );

      const glm::vec3 sample{ 5, 8, 9 };
      const glm::vec3 glmPoint = glmTransform * glm::vec4{ sample.x, sample.y, sample.z, 1 };
      const glm::vec3 glmVec = glmTransform * glm::vec4{ sample.x, sample.y, sample.z, 0 };
      const glm::vec3 ptPoint3 = transform.transformPoint(sample);
      const glm::vec2 ptPoint2 = transform.transformPoint(glm::vec2{ sample.x, sample.y });
      const glm::vec3 ptVec3 = transform.transformVector(sample);
      const glm::vec2 ptVec2 = transform.transformVector(glm::vec2{ sample.x, sample.y });

      eq(glmPoint, ptPoint3);
      eq(glmPoint, ptPoint2);
      eq(glmVec, ptVec3);
      eq(glmVec, ptVec2);

      const glm::mat4 glmInvTransform = glm::inverse(glmTransform);
      const Transform::PackedTransform invPtTransform = transform.inverse();
      const glm::vec3 glmInvPoint = glmInvTransform * glm::vec4{ sample.x, sample.y, sample.z, 1.f };
      const glm::vec3 ptInvPoint = invPtTransform.transformPoint(sample);

      eq(glmInvPoint, ptInvPoint);

      const glm::mat4 glmComposed = glmInvTransform * glmTransform;
      const Transform::PackedTransform ptComposed = invPtTransform * transform;

      eq(glmComposed * glm::vec4{ sample.x, sample.y, sample.z, 1.f }, ptComposed.transformPoint(sample));

      const Transform::Parts parts = transform.decompose();
      eq(pos, parts.translate);
      eq(glm::vec3{ std::cos(rotRad), std::sin(rotRad), 0.f }, parts.rot);
      eq(scale, parts.scale);
    }

    TEST_METHOD(Default) {
      eq(glm::vec3{1, 2, 3 }, Transform::PackedTransform{}.transformPoint(glm::vec3{ 1, 2, 3 }));
      eq(glm::vec3{1, 2, 3 }, Transform::PackedTransform::build({}).transformPoint(glm::vec3{ 1, 2, 3 }));
    };
  };
}