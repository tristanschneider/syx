#include "Precompile.h"
#include "asset/AssetRepo.h"
#include "asset/Asset.h"
#include "asset/AssetLoader.h"
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"
#include "App.h"

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

AssetRepo::AssetRepo(App& app)
  : System(app)
  , mPool(app.getWorkerPool()) {
}

AssetRepo::~AssetRepo() {
}

std::shared_ptr<Asset> AssetRepo::getAsset(AssetInfo info) {
  _fillInfo(info);
  //Get or insert in asset map
  std::shared_ptr<Asset> newAsset;
  {
    std::unique_lock<std::mutex> lock(mAssetMutex);
    auto it = mIdToAsset.find(info.mId);
    while(it != mIdToAsset.end() && it->second->getInfo().mUri == info.mUri)
      it = mIdToAsset.find(++info.mId);

    if(it != mIdToAsset.end())
      return it->second;
    //If uri wasn't given then there's nothing to create the asset from
    if(info.mUri.empty())
      return nullptr;

    newAsset = Loaders::getAsset(std::move(info));
    if(!newAsset)
      return nullptr;

    mIdToAsset[info.mId] = newAsset;
  }

  //Queue loading of asset
  mPool.queueTask(std::make_shared<FunctionTask>([newAsset, this](){
    std::unique_ptr<AssetLoader> loader = _getLoader(newAsset->getInfo().mCategory);
    AssetLoadResult result = loader->load(mBasePath, *newAsset);
    _assetLoaded(result, *newAsset, *loader);
    _returnLoader(std::move(loader));
  }));

  return newAsset;
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
    loader.postProcess(mApp, asset);
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

std::unique_ptr<AssetLoader> AssetRepo::_getLoader(const std::string& category) {
  //Attempt to re-use an existing loader in the pool, and fall back to creating a new one if none are found
  std::unique_lock<std::mutex> lock(mLoaderMutex);
  auto loadersIt = mLoaderPool.find(category);
  if(loadersIt != mLoaderPool.end()) {
    auto& loaders = loadersIt->second;
    if(loaders.size()) {
      std::unique_ptr<AssetLoader> result = std::move(loaders.back());
      loaders.pop_back();
      return result;
    }
  }
  return Loaders::getLoader(category);
}

void AssetRepo::_returnLoader(std::unique_ptr<AssetLoader> loader) {
  //Return a loader to the pool unless that category is already full
  std::unique_lock<std::mutex> lock(mLoaderMutex);
  const std::string& category = loader->getCategory();
  auto loadersIt = mLoaderPool.find(category);
  if(loadersIt != mLoaderPool.end()) {
    if(loadersIt->second.size() < sMaxLoaders)
      loadersIt->second.emplace_back(std::move(loader));
    //else pool is full, let loader destruct
  }
  else {
    std::vector<std::unique_ptr<AssetLoader>> loaderContainer;
    loaderContainer.emplace_back(std::move(loader));
    mLoaderPool[category] = std::move(loaderContainer);
  }
}