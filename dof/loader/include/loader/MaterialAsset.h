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
    auto operator<=>(const TextureAsset&) const = default;

    size_t width{};
    size_t height{};
    TextureSampleMode sampleMode{};
    TextureFormat format{};
    std::vector<uint8_t> buffer;
  };

  struct MaterialAsset {
    auto operator<=>(const MaterialAsset&) const = default;

    TextureAsset texture;
  };
}