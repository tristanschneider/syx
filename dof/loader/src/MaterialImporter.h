#pragma once

namespace Loader {
  struct AssetVariant;
  class IAssetImporter;

  enum class MaterialImportSampleMode {
    Linear,
    SnapToNearest,
    //Use linear for larger textures and snap for smaller ones
    //The assumption is that small is pixel art
    GuessFromSize
  };

  struct RawMaterial {
    //Must be width*height*4
    const uint8_t* bytes{};
    size_t width{};
    size_t height{};
    MaterialImportSampleMode sampleMode{};
  };

  void materialFromRaw(const RawMaterial& raw, AssetVariant& result);

  std::unique_ptr<IAssetImporter> createMaterialImporter(MaterialImportSampleMode mode);
}