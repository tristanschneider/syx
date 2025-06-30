#include "Precompile.h"
#include "CppUnitTest.h"

#include <ConvexHull.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(ConvexHullTest) {
    struct TestContext {
      operator ConvexHull::Context&() {
        return ctx;
      }

      void assertMatch(const std::vector<int>& expected) {
        std::vector<uint32_t> e(expected.size());
        std::transform(expected.begin(), expected.end(), e.begin(), [](int i) { return static_cast<uint32_t>(i); });
        Assert::IsTrue(e == ctx.result);
      }

      ConvexHull::Context ctx;
    };

    TEST_METHOD(Point_IsOwnHull) {
      TestContext ctx;
      ConvexHull::compute(std::vector{ glm::vec2{ 1, 2 } }, ctx);
      ctx.assertMatch({ 0 });
    }

    TEST_METHOD(Line_IsOwnHull) {
      TestContext ctx;
      ConvexHull::compute(std::vector{ glm::vec2{ 1, 2 }, glm::vec2{ 3, 4 } }, ctx);
      ctx.assertMatch({ 0, 1 });
    }

    TEST_METHOD(CCWTri_IsOwnHull) {
      TestContext ctx;
      ConvexHull::compute(std::vector{ glm::vec2{ 1, 2 }, glm::vec2{ 2, 2 }, glm::vec2{ 2, 3 } }, ctx);
      ctx.assertMatch({ 0, 1, 2 });
    }

    TEST_METHOD(CWTri_IsReordered) {
      TestContext ctx;
      ConvexHull::compute(std::vector{ glm::vec2{ 1, 2 }, glm::vec2{ 2, 3 }, glm::vec2{ 2, 2 } }, ctx);
      ctx.assertMatch({ 0, 2, 1 });
    }

    TEST_METHOD(MixedQuad_IsReordered) {
      TestContext ctx;
      ConvexHull::compute(std::vector{
        glm::vec2{ 1, 1 },
        glm::vec2{ -1, -1 },
        glm::vec2{ -1, 1 },
        glm::vec2{ 1, -1 },
      }, ctx);
      ctx.assertMatch({ 1, 3, 0, 2 });
    }

    TEST_METHOD(MixedQuadWithExtra_AreDiscarded) {
      TestContext ctx;
      ConvexHull::compute(std::vector{
        glm::vec2{ 0, 0 }, //0
        glm::vec2{ 1, 1 }, //1
        glm::vec2{ 0, 0 }, //2
        glm::vec2{ 0.1f, 0.1f }, //3
        glm::vec2{ -1, -1 }, //4
        glm::vec2{ -0.1f, -0.1f }, //5
        glm::vec2{ -1, 1 }, //6
        glm::vec2{ 1, 0 }, //7
        glm::vec2{ 1, -1 }, //8
        glm::vec2{ -0.3f, -0.1f }, //9
      }, ctx);
      ctx.assertMatch({ 4, 8, 1, 6 });
    }
  };
}
