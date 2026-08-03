#include <CppUnitTest.h>

extern "C" {
#include <sandbox/SandGrid.h>
}
#include <c/TestAllocator.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  struct GridLiteral {
    //template<size_t Y, size_t X>
    //static GridLiteral fromArray(const uint8_t (&g)[Y][X]) {
    GridLiteral(std::vector<uint8_t> g, size_t _stride)
      : data{ std::move(g) }
      , stride{ _stride }
    {
    }

    size_t _index(int x, int y) const {
      return x + y * stride;
    }

    void set(int x, int y, uint8_t v) {
      data[_index(x, y)] = v;
    }

    uint8_t get(int x, int y) const {
      return data[_index(x, y)];
    }

    std::string str() {
      const size_t rowCount = data.size() / stride;
      std::string result(data.size() + rowCount, ' ');
      size_t cursor{};
      for(size_t i = 0; i < data.size(); ++i) {
        if((i % stride) == 0) {
          result[cursor++] = '\n';
        }
        result[cursor++] = '0' + data[i];
      }

      return result;
    }

    std::vector<uint8_t> data;
    size_t stride{};
  };

  template<size_t Y, size_t X>
  static GridLiteral makeGrid(const uint8_t (&g)[Y][X]) {
    std::vector<uint8_t> grid(Y*X);
    std::memcpy(grid.data(), g, grid.size());
    return GridLiteral{ std::move(grid), X };
  }

  struct TestGrid {
    TestGrid(sbx_SandGridConfig cfg)
      : config{ cfg }
      , grid{ sbx_SandGrid_ctor(alloc.get(), cfg) } {
    }

    TestGrid(int width, int height)
      : TestGrid{
          sbx_SandGridConfig{
            .width = width,
            .height = height,
            //Just enough to move between cells in one tick
            .gravity = clm_vec2_ctor(0, -1.1f)
          }
        }
    {
    }

    ~TestGrid() {
      std_Allocator_dealloc(alloc.get(), grid);
      grid = nullptr;
    }

    void set(int x, int y, int mass) {
      sbx_SandGridGrain grain{
        .mass = static_cast<uint8_t>(mass),
        .shape = { sbx_GrainType::SBX_GT_GRAIN },
        .color = clm_byte4_ctor(static_cast<uint8_t>(mass), 0, 0, 0)
      };
      clm_irect rect = clm_irect_fromMinMax(x, y, x + 1, y + 1);
      sbx_SandGridInsertOps ops{
        .grid = grid,
        .grains = &grain,
        .grainCount = 1,
        .rect = &rect,
        .mode = sbx_SandGridInsertMode::SBX_SGI_REPLACE
      };
      sbx_SandGrid_insert(&ops);
    }

    void integrate() {
      clm_irect rect = clm_irect_limits();
      sbx_SandGrid_integrate(grid, &rect, 1.f);
    }

    std::string str() {
      const size_t grains = config.width * config.height;
      std::vector<uint8_t> gridBytes(grains);
      const clm_byte4* tex = sbx_SandGrid_getTexture(grid);
      Assert::IsNotNull(tex);
      for(size_t i = 0; i < grains; ++i) {
        gridBytes[i] = tex[i].r;
      }
      return GridLiteral{ std::move(gridBytes), static_cast<size_t>(config.width) }.str();
    }

    sbx_SandGridConfig config;
    TestAllocator alloc;
    sbx_SandGrid* grid;
  };

  TEST_CLASS(SandGridTest) {
    TEST_METHOD(Init) {
      TestGrid grid{ 3, 2 };
      uint8_t ex[2][3] = {
        { 0, 0, 0 },
        { 0, 0, 0 }
      };
      Assert::AreEqual(makeGrid(ex).str(), grid.str());
      grid.set(0, 0, 1);
      grid.set(2, 0, 2);
      grid.set(2, 1, 3);
      ex[0][0] = 1;
      ex[0][2] = 2;
      ex[1][2] = 3;
      Assert::AreEqual(makeGrid(ex).str(), grid.str());
      grid.set(0, 0, 0);
      ex[0][0] = 0;
      Assert::AreEqual(makeGrid(ex).str(), grid.str());
    }

    TEST_METHOD(Integrate) {
      TestGrid grid{ 3, 3 };
      grid.set(1, 0, 1);
      uint8_t ex[3][3] = { 0 };
      ex[0][1] = 1;

      Assert::AreEqual(makeGrid(ex).str(), grid.str());

      grid.integrate();
      std::swap(ex[0][1], ex[1][1]);
      Assert::AreEqual(makeGrid(ex).str(), grid.str());

      grid.integrate();
      std::swap(ex[1][1], ex[2][1]);
      Assert::AreEqual(makeGrid(ex).str(), grid.str());

      grid.integrate();
      Assert::AreEqual(makeGrid(ex).str(), grid.str());
    }
  };
}