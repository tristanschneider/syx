#include "Precompile.h"
#include "ecs/system/AssetSystem.h"

#include "ecs/component/GraphicsComponents.h"

namespace Assets {
  using namespace Engine;
  using IOFailView = View<Include<AssetLoadRequestComponent>, Include<FileReadFailureResponse>>;
  using AssetFailView = View<Include<AssetLoadFailedComponent>>;
  void tickDelete(SystemContext<IOFailView, AssetFailView, EntityFactory>& context) {
    auto& io = context.get<IOFailView>();
    auto& failedAssets = context.get<AssetFailView>();
    EntityFactory factory = context.get<EntityFactory>();
    while(auto first = io.tryGetFirst()) {
      factory.destroyEntity(first->entity());
    }
    while(auto first = failedAssets.tryGetFirst()) {
      factory.destroyEntity(first->entity());
    }
  }

  using RequestView = View<Read<AssetLoadRequestComponent>, Exclude<FileReadRequest>>;
  using RequestModifier = EntityModifier<FileReadRequest, AssetInfoComponent>;
  void tickRequests(SystemContext<RequestView, RequestModifier>& context) {
    RequestModifier modifier = context.get<RequestModifier>();
    for(auto request : context.get<RequestView>()) {

      auto entity = request.entity();
      FilePath path = request.get<const AssetLoadRequestComponent>().mPath;
      modifier.addComponent<FileReadRequest>(entity, path);
      modifier.addComponent<AssetInfoComponent>(entity, std::move(path));
    }
  }

  constexpr int sDataPosOffset = 0x0A;
  constexpr int sImageSizeOffset = 0x22;
  constexpr int sWidthOffset = 0x12;
  constexpr int sHeightOffset = 0x16;
  constexpr size_t sHeaderSize = 54;

  AssetLoadResultV<TextureComponent> loadTexture(const AssetInfoComponent& info, std::vector<uint8_t>& buffer) {
    if(std::strcmp(info.mPath.getExtensionWithoutDot(), "bmp")) {
      return UnsupportedAssetTag{};
    }
    if(buffer.size() < sHeaderSize) {
      printf("Error reading header of bmp at %s\n", info.mPath.cstr());
      return FailedAssetTag{};
    }

    if(buffer[0] != 'B' || buffer[1] != 'M') {
      printf("Not a bmp file at %s\n", info.mPath.cstr());
      return FailedAssetTag{};
    }

    uint32_t dataStart = reinterpret_cast<uint32_t&>(buffer[sDataPosOffset]);
    uint32_t imageSize = reinterpret_cast<uint32_t&>(buffer[sImageSizeOffset]);
    uint16_t width = reinterpret_cast<uint16_t&>(buffer[sWidthOffset]);
    uint16_t height = reinterpret_cast<uint16_t&>(buffer[sHeightOffset]);

    //If the fields are missing, fill them in
    if(!imageSize)
      imageSize=width*height*3;
    if(!dataStart)
      dataStart = 54;

    if(buffer.size() - dataStart < imageSize) {
      printf("Invalid bmp data at %s\n", info.mPath.cstr());
      return FailedAssetTag{};
    }

    //Resize in preparation for the fourth component we'll add to each pixel
    std::vector<uint8_t> temp(width*height*4);
    for(uint16_t i = 0; i < width*height; ++i) {
      size_t bp = 4*i;
      size_t tp = 3*i + dataStart;
      //Flip bgr to rgb and add empty alpha
      temp[bp] = buffer[tp + 2];
      temp[bp + 1] = buffer[tp + 1];
      temp[bp + 2] = buffer[tp];
      //TODO: waste of memory, upload to gpu as rgb
      temp[bp + 3] = 0;
    }

    TextureComponent result;
    result.mWidth = width;
    result.mHeight = height;
    //TODO: could write directly into buffer and get rid of temporary
    result.mBuffer = std::move(temp);
    return result;
  }
}

std::shared_ptr<Engine::System> AssetSystem::processLoadRequests() {
  return ecx::makeSystem("ProcessAssetLoadRequest", &Assets::tickRequests);
}

std::shared_ptr<Engine::System> AssetSystem::deleteFailedAssets() {
  return ecx::makeSystem("DeleteFailedAssets", &Assets::tickDelete);
}

std::shared_ptr<Engine::System> AssetSystem::createGraphicsModelLoader() {
  //TODO:
  return nullptr;
}

std::shared_ptr<Engine::System> AssetSystem::createShaderLoader() {
  //TODO:
  return nullptr;
}

std::shared_ptr<Engine::System> AssetSystem::createTextureLoader() {
  return intermediateAssetLoadSystem<&Assets::loadTexture, NeedsGpuUploadComponent>("bmpLoader");
}
