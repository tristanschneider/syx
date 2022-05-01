#include "Precompile.h"
#include "ecs/system/AssetSystem.h"

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
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

  AssetLoadResultV<GraphicsModelComponent> loadModel(const AssetInfoComponent& info, std::vector<uint8_t>& buffer) {
    //TODO: maybe store this somewhere so it doesn't need to be reallocated every time
    Assimp::Importer importer;
    if(!importer.IsExtensionSupported(info.mPath.getExtensionWithDot())) {
      return UnsupportedAssetTag{};
    }
    const aiScene* scene = importer.ReadFileFromMemory(buffer.data(), buffer.size(), unsigned int(aiProcess_JoinIdenticalVertices
      | aiProcess_Triangulate
      | aiProcess_GenNormals
      // Flatten hierarchy since hierarchy support not implemented yet
      | aiProcess_PreTransformVertices
      | aiProcess_GenUVCoords
      | aiProcess_GenBoundingBoxes)
    );

    if(!scene || !scene->mNumMeshes) {
      printf("Error loading model %s, [%s]\n", info.mPath.cstr(), importer.GetErrorString());
      return FailedAssetTag{};
    }

    GraphicsModelComponent result;
    const auto [ totalVerts, totalIndices ] = std::invoke([&]{
      std::pair<size_t, size_t> result{ 0, 0 };
      for(unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[i];
        result.first += static_cast<size_t>(mesh->mNumVertices);
        result.second += static_cast<size_t>(mesh->mNumFaces * 3);
      }
      return result;
    });
    if(!totalVerts && !totalIndices) {
      printf("No model data found %s\n", info.mPath.cstr());
      return FailedAssetTag{};
    }
    result.mVertices.reserve(totalVerts);
    result.mIndices.reserve(totalIndices);

    for(unsigned int i = 0; i < scene->mNumMeshes; ++i) {
      const aiMesh* mesh = scene->mMeshes[i];
      const uint32_t baseVertex = static_cast<uint32_t>(result.mVertices.size());
      for(unsigned int v = 0; v < mesh->mNumVertices; ++v) {
        GraphicsModelComponent::Vertex vert;
        constexpr size_t VERT_SIZE = sizeof(float) * 3;
        static_assert(sizeof(aiVector3D) == VERT_SIZE);

        std::memcpy(&vert.mPos[0], &mesh->mVertices[v], VERT_SIZE);
        std::memcpy(&vert.mNormal[0], &mesh->mNormals[v], VERT_SIZE);
        std::memcpy(&vert.mUV[0], &mesh->mTextureCoords[v], sizeof(float) * 2);
        result.mVertices.push_back(vert);
      }
      for(unsigned int f = 0; f < mesh->mNumFaces; ++f) {
        const aiFace& face = mesh->mFaces[f];
        assert(face.mNumIndices == 3 && "aiProcess_Triangulate is supposed to result in triangles only");
        for(unsigned int vi = 0; vi < face.mNumIndices; ++vi) {
          result.mIndices.push_back(static_cast<uint32_t>(face.mIndices[vi] + baseVertex));
        }
      }
    }

    return result;
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
  return intermediateAssetLoadSystem<&Assets::loadModel, NeedsGpuUploadComponent>("modelLoader");
}

std::shared_ptr<Engine::System> AssetSystem::createShaderLoader() {
  //TODO:
  return nullptr;
}

std::shared_ptr<Engine::System> AssetSystem::createTextureLoader() {
  return intermediateAssetLoadSystem<&Assets::loadTexture, NeedsGpuUploadComponent>("bmpLoader");
}
