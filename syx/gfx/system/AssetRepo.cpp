#include "Precompile.h"
#include "system/AssetRepo.h"
#include "asset/Asset.h"
#include "loader/AssetLoader.h"
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"

RegisterSystemCPP(AssetRepo);

AssetRepo::Loaders::Loaders() {
}

AssetRepo::Loaders::~Loaders() {
}

void AssetRepo::Loaders::registerLoader(const std::string& category, LoaderConstructor constructLoader, AssetConstructor constructAsset) {
  //Duplicate loaders registered, likely undefined behavior
  assert(_get().mCategoryToConstructors.find(category) == _get().mCategoryToConstructors.end());
  _get().mCategoryToConstructors[category] = { constructLoader, constructAsset };
}

std::unique_ptr<AssetLoader> AssetRepo::Loaders::getLoader(const std::string& category) {
  auto it = _get().mCategoryToConstructors.find(category);
  return it != _get().mCategoryToConstructors.end() ? it->second.first() : nullptr;
}

std::shared_ptr<Asset> AssetRepo::Loaders::getAsset(AssetInfo&& info) {
  auto it = _get().mCategoryToConstructors.find(info.mCategory);
  return it != _get().mCategoryToConstructors.end() ? it->second.second(std::move(info)) : nullptr;
}

AssetRepo::Loaders& AssetRepo::Loaders::_get() {
  static Loaders singleton;
  return singleton;
}

AssetRepo::AssetRepo(const SystemArgs& args)
  : System(args) {
}

AssetRepo::~AssetRepo() {
}

std::shared_ptr<Asset> AssetRepo::getAsset(AssetInfo info) {
  _fillInfo(info);
  //Get or insert in asset map
  size_t prevId = info.mId;
  {
    auto readLock = mAssetLock.getReader();
    if(std::shared_ptr<Asset> existing = _find(info))
      return existing;
    //If uri wasn't given then there's nothing to create the asset from
    if(info.mUri.empty())
      return nullptr;
  }

  //Reset id change for when we look up again
  info.mId = prevId;

  //Asset doesn't exist, grab write lock to create the asset
  std::shared_ptr<Asset> newAsset;
  {
    auto writeLock = mAssetLock.getWriter();
    //Need to make sure asset didn't get created while acquiring the lock
    std::shared_ptr<Asset> existing = _find(info);
    if(existing)
      return existing;

    newAsset = Loaders::getAsset(std::move(info));
    if(!newAsset)
      return nullptr;

    mIdToAsset[info.mId] = newAsset;
  }

  _queueLoad(newAsset);
  return newAsset;
}

std::shared_ptr<Asset> AssetRepo::_find(AssetInfo& info) {
  auto it = mIdToAsset.find(info.mId);
  while (it != mIdToAsset.end() && it->second->getInfo().mUri == info.mUri)
    it = mIdToAsset.find(++info.mId);
  return it != mIdToAsset.end() ? it->second : nullptr;
}

void AssetRepo::_queueLoad(std::shared_ptr<Asset> asset) {
  mArgs.mPool->queueTask(std::make_shared<FunctionTask>([asset, this]() {
    if(AssetLoader* loader = _getLoader(asset->getInfo().mCategory)) {
      //Locking here is overkill, but makes it less easier to forget in a particular loader
      //Unlikely to cause blocks as users can check the status of the asset against Loaded or PostProccessed
      auto lock = asset->getLock().getWriter();
      AssetLoadResult result = loader->load(mBasePath, *asset);
      _assetLoaded(result, *asset, *loader);
    }
  }));
}

void AssetRepo::reloadAsset(std::shared_ptr<Asset> asset) {
  asset->mState = AssetState::Empty;
  _queueLoad(asset);
}

void AssetRepo::setBasePath(const std::string& basePath) {
  mBasePath = basePath;
}

void AssetRepo::_assetLoaded(AssetLoadResult result, Asset& asset, AssetLoader& loader) {
  switch(result) {
  case AssetLoadResult::NotFound:
    printf("Failed to find asset at location %s\n", asset.getInfo().mUri.c_str());
    break;
  case AssetLoadResult::Fail:
    printf("Failed to load asset at location %s\n", asset.getInfo().mUri.c_str());
    break;
  case AssetLoadResult::IOError:
    printf("IO error attempting to load asset at location %s\n", asset.getInfo().mUri.c_str());
    break;
  case AssetLoadResult::Success:
    asset.mState = AssetState::Loaded;
    loader.postProcess(mArgs, asset);
    break;
  }
}

void AssetRepo::_fillInfo(AssetInfo& info) {
  if(!info.mUri.empty()) {
    std::hash<std::string> hash;
    info.mId = hash(info.mUri);
    info.mCategory = AssetInfo::getCategory(info.mUri);
  }
}

AssetLoader* AssetRepo::_getLoader(const std::string& category) {
  auto& loaders = mLoaderPool.get();
  auto it = loaders.find(category);
  //If a loader already exists, use that one
  if(it != loaders.end())
    return it->second.get();

  //Loader doesn't exist, make a new one, store it in the pool and return it
  std::unique_ptr<AssetLoader> newLoader = Loaders::getLoader(category);
  AssetLoader* result = newLoader.get();
  loaders[category] = std::move(newLoader);
  return result;
}