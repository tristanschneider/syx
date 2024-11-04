#include "Precompile.h"

namespace Loader {
  enum class TextureSampleMode : uint8_t {
    SnapToNearest,
    LinearInterpolation
  };

  enum class TextureFormat : uint8_t {
    RGB
  };

  struct TextureAsset {
    size_t width{};
    size_t height{};
    TextureSampleMode sampleMode{};
    TextureFormat format{};
    std::vector<uint8_t> buffer;
  };

  struct MaterialAsset {
    TextureAsset texture;
  };
}