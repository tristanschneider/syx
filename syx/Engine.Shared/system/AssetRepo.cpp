#include "Precompile.h"
#include "system/AssetRepo.h"
#include "asset/Asset.h"
#include "event/AssetEvents.h"
#include "event/EventBuffer.h"
#include "loader/AssetLoader.h"
#include "provider/MessageQueueProvider.h"
#include "threading/FunctionTask.h"
#include "threading/IWorkerPool.h"

class AssetLoaderRegistry : public IAssetLoaderRegistry {
public:
  void registerLoader(const std::string& category, LoaderConstructor constructLoader, AssetConstructor constructAsset) override {
    //Duplicate loaders registered, likely undefined behavior
    assert(mCategoryToConstructors.find(category) == mCategoryToConstructors.end());
    mCategoryToConstructors[category] = { std::move(constructLoader), std::move(constructAsset) };
  }

  std::unique_ptr<AssetLoader> getLoader(FileSystem::IFileSystem& fileSystem, const std::string& category) override {
    auto it = mCategoryToConstructors.find(category);
    return it != mCategoryToConstructors.end() ? it->second.first(fileSystem) : nullptr;
  }

  std::shared_ptr<Asset> getAsset(AssetInfo&& info) override {
    auto it = mCategoryToConstructors.find(info.mCategory);
    return it != mCategoryToConstructors.end() ? it->second.second(std::move(info)) : nullptr;
  }

private:
  std::unordered_map<std::string, std::pair<LoaderConstructor, AssetConstructor>> mCategoryToConstructors;
};

AssetRepo::AssetRepo(const SystemArgs& args, std::unique_ptr<IAssetLoaderRegistry> loaderRegistry)
  : System(args, _typeId<AssetRepo>())
  , mLoaderRegistry(std::move(loaderRegistry)) {
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

    newAsset = mLoaderRegistry->getAsset(std::move(info));
    if(!newAsset)
      return nullptr;

    mIdToAsset[info.mId] = newAsset;
  }

  _queueLoad(newAsset);
  return newAsset;
}

void AssetRepo::init() {
  mEventHandler = std::make_unique<EventHandler>();
  _registerSystemEventHandler(&AssetRepo::_onAssetRequest);
  _registerSystemEventHandler(&AssetRepo::_onReloadRequest);
  _registerSystemEventHandler(&AssetRepo::_onAssetQueryRequest);
  _registerSystemEventHandler(&AssetRepo::_onAddAssetRequest);
}

void AssetRepo::queueTasks(float, IWorkerPool& pool, std::shared_ptr<Task> frameTask) {
  auto handleEvents = std::make_shared<FunctionTask>([this] {
    mEventHandler->handleEvents(*mEventBuffer);
  });
  handleEvents->then(frameTask);
  pool.queueTask(std::move(handleEvents));
}

void AssetRepo::getAssetsByCategory(std::string_view category, std::vector<std::shared_ptr<Asset>>& assets) const {
  auto lock = mAssetLock.getReader();
  for(const auto& it : mIdToAsset) {
    if(it.second->getState() >= AssetState::Loaded && it.second->getInfo().mCategory == category) {
      assets.emplace_back(it.second);
    }
  }
}

void AssetRepo::addAsset(std::shared_ptr<Asset> asset) {
  AssetInfo& info = asset->mInfo;
  _fillInfo(info);
  auto lock = mAssetLock.getWriter();
  assert(asset && asset->getInfo().mUri.size() && "Asset must exist and have a uri");
  auto it = mIdToAsset.find(info.mId);
  while (it != mIdToAsset.end()) {
    assert(it->second->getInfo().mUri != info.mUri && "Asset should not already exist when adding it");
    it = mIdToAsset.find(++info.mId);
  }
  asset->mState = AssetState::Loaded;
  mIdToAsset[info.mId] = std::move(asset);
}

void AssetRepo::forEachAsset(const std::function<void(std::shared_ptr<Asset>)> callback) {
  auto lock = mAssetLock.getReader();
  for(const auto& it : mIdToAsset) {
    //TODO: how to protect against recursive lock from callback?
    callback(it.second);
  }
}

void AssetRepo::removeAsset(AssetInfo info) {
  info.fill();
  auto lock = mAssetLock.getWriter();
  auto it = mIdToAsset.find(info.mId);
  if(it != mIdToAsset.end()) {
    mIdToAsset.erase(it);
  }
}

std::shared_ptr<Asset> AssetRepo::_find(AssetInfo& info) {
  auto it = mIdToAsset.find(info.mId);
  //Deal with hash collisions by incrementing id if a uri was provided
  while (it != mIdToAsset.end() && !info.mUri.empty() && it->second->getInfo().mUri != info.mUri)
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
      _assetLoaded(result, asset, *loader);
    }
  }));
}

void AssetRepo::_onAssetRequest(const GetAssetRequest& e) {
  e.respond(mArgs.mMessages->getMessageQueue(), GetAssetResponse(getAsset(e.mInfo)));
}

void AssetRepo::_onReloadRequest(const ReloadAssetRequest& e) {
  auto existingAsset = getAsset(AssetInfo(e.mInfo.mId));
  const bool isNewAsset = !existingAsset;
  if(isNewAsset) {
    existingAsset = getAsset(e.mInfo);
  }
  e.respond(mArgs.mMessages->getMessageQueue(), ReloadAssetResponse(existingAsset, isNewAsset));
}

void AssetRepo::_onAssetQueryRequest(const AssetQueryRequest& e) {
  AssetQueryResponse result;
  //Empty is a special case that means get them all
  if(e.mCategory.empty()) {
    forEachAsset([&result](std::shared_ptr<Asset> asset) {
      result.mResults.emplace_back(asset);
    });
  }
  else {
    getAssetsByCategory(e.mCategory, result.mResults);
  }
  e.respond(mArgs.mMessages->getMessageQueue(), std::move(result));
}

void AssetRepo::_onAddAssetRequest(const AddAssetRequest& e) {
  addAsset(e.mAsset);
}

void AssetRepo::reloadAsset(std::shared_ptr<Asset> asset) {
  //TODO: this probably doesn't work properly if the asset was already in the middle of loading
  {
    auto lock(asset->getLock().getWriter());
    asset->mState = AssetState::Empty;
  }
  _queueLoad(asset);
}

void AssetRepo::setBasePath(const std::string& basePath) {
  mBasePath = basePath;
}

void AssetRepo::_assetLoaded(AssetLoadResult result, std::shared_ptr<Asset> asset, AssetLoader& loader) {
  switch(result) {
  case AssetLoadResult::NotFound:
    printf("Failed to find asset at location %s\n", asset->getInfo().mUri.c_str());
    break;
  case AssetLoadResult::Fail:
    printf("Failed to load asset at location %s\n", asset->getInfo().mUri.c_str());
    break;
  case AssetLoadResult::IOError:
    printf("IO error attempting to load asset at location %s\n", asset->getInfo().mUri.c_str());
    break;
  case AssetLoadResult::Success:
    asset->mState = AssetState::Loaded;
    loader.postProcess(mArgs, asset);
    break;
  }
}

void AssetRepo::_fillInfo(AssetInfo& info) {
  info.fill();
}

AssetLoader* AssetRepo::_getLoader(const std::string& category) {
  auto& loaders = mLoaderPool.get();
  auto it = loaders.find(category);
  //If a loader already exists, use that one
  if(it != loaders.end())
    return it->second.get();

  //Loader doesn't exist, make a new one, store it in the pool and return it
  std::unique_ptr<AssetLoader> newLoader = mLoaderRegistry->getLoader(*mArgs.mFileSystem, category);
  AssetLoader* result = newLoader.get();
  loaders[category] = std::move(newLoader);
  return result;
}

namespace Registry {
  std::unique_ptr<IAssetLoaderRegistry> createAssetLoaderRegistry() {
    return std::make_unique<AssetLoaderRegistry>();
  }
}