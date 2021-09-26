#include "Precompile.h"
#include "loader/ShaderLoader.h"

#include "asset/Shader.h"
#include "graphics/RenderCommand.h"
#include "provider/MessageQueueProvider.h"
#include "system/System.h"

ShaderLoader::~ShaderLoader() = default;

AssetLoadResult ShaderLoader::load(const std::string& basePath, Asset& asset) {
  std::string vsPath = basePath + asset.getInfo().mUri;
  AssetLoadResult vsResult = _readEntireFile(vsPath, mSourceVS);
  //Turn .vs extension into .ps
  vsPath[vsPath.length() - 2] = 'p';
  AssetLoadResult psResult = _readEntireFile(vsPath, mSourcePS);
  static_cast<Shader&>(asset).set(std::move(mSourceVS), std::move(mSourcePS));
  if(vsResult != AssetLoadResult::Success)
    return vsResult;
  return psResult;
}

void ShaderLoader::postProcess(const SystemArgs& args, std::shared_ptr<Asset> asset) {
  args.mMessages->getMessageQueue()->push(DispatchToRenderThreadEvent([asset] {
    static_cast<Shader&>(*asset).load();
  }));
}
