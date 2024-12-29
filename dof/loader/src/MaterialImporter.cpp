#include "Precompile.h"
#include "MaterialImporter.h"

#include "IAssetImporter.h"
#include "STBInterface.h"
#include "generics/Hash.h"
#include "loader/MaterialAsset.h"
#include "AssetVariant.h"
#include "AssetTables.h"

namespace Loader {
  struct AssetLoadTask;
  struct LoadRequest;

  struct MaterialContext {
    MaterialImportSampleMode sampleMode{};
    AssetVariant& result;
  };

  constexpr int CHANNELS = 4;

  void materialFromRaw(const RawMaterial& raw, AssetVariant& result) {
    //This is null when stb load fails
    if(!raw.bytes) {
      return;
    }
    TextureAsset& t = result->emplace<MaterialAsset>().texture;
    t.format = TextureFormat::RGBA;
    t.width = raw.width;
    t.height = raw.height;
    t.buffer.resize(t.width*t.height*CHANNELS);
    std::memcpy(t.buffer.data(), raw.bytes, t.buffer.size());

    switch(raw.sampleMode) {
      case MaterialImportSampleMode::Linear:
        t.sampleMode = TextureSampleMode::LinearInterpolation;
        break;
      case MaterialImportSampleMode::SnapToNearest:
        t.sampleMode = TextureSampleMode::SnapToNearest;
        break;
      case MaterialImportSampleMode::GuessFromSize:
        t.sampleMode = t.width * t.height > 128*128 ? TextureSampleMode::LinearInterpolation : TextureSampleMode::SnapToNearest;
    }
  }

  void stbLoad(ImageData&& data, MaterialContext& ctx) {
    materialFromRaw(RawMaterial{
      .bytes = reinterpret_cast<uint8_t*>(data.mBytes),
      .width = data.mWidth,
      .height = data.mHeight,
      .sampleMode = ctx.sampleMode
    }, ctx.result);

    STBInterface::deallocate(std::move(data));
  }

  class MaterialImporter : public IAssetImporter {
  public:
    MaterialImporter(MaterialImportSampleMode mode)
      : sampleMode{ mode }
    {
    }

    bool isSupportedExtension(std::string_view extension) final {
      switch(gnx::Hash::constHash(extension)) {
      case gnx::Hash::constHash("jpeg"):
      case gnx::Hash::constHash("JPEG"):
      case gnx::Hash::constHash("png"):
      case gnx::Hash::constHash("PNG"):
      case gnx::Hash::constHash("bmp"):
      case gnx::Hash::constHash("BMP"):
      case gnx::Hash::constHash("tga"):
      case gnx::Hash::constHash("TGA"):
        return true;
      default:
        return false;
      }
    }

    void loadAsset(const Loader::LoadRequest& request, AssetVariant& result) final {
      MaterialContext ctx{
        .sampleMode = sampleMode,
        .result = result
      };
      if(request.contents.size()) {
        stbLoad(STBInterface::loadImageFromBuffer(reinterpret_cast<const unsigned char*>(request.contents.data()), request.contents.size(), CHANNELS), ctx);
      }
      else {
        stbLoad(STBInterface::loadImageFromFile(request.location.filename.c_str(), CHANNELS), ctx);
      }
    }

    MaterialImportSampleMode sampleMode{};
  };

  std::unique_ptr<IAssetImporter> createMaterialImporter(MaterialImportSampleMode mode) {
    return std::make_unique<MaterialImporter>(mode);
  }
}