#pragma once
//Repository that manages acquisition and async loading of assets.
//AssetLoaders register themselves through AssetRepo::Loaders::registerLoader
//getAsset always returns an asset. This is either a previously loaded asset, or a
//newly created empty asset that will be soon loaded with the given loader in a task.
//Loaders are pooled so resources can be re-used in the same loader between loading of different assets.

#include "asset/Asset.h"
#include "system/System.h"
#include "threading/ThreadLocal.h"

class IWorkerPool;
class Asset;
class AssetLoader;
struct AssetInfo;
enum class AssetLoadResult : uint8_t;
class IAssetLoaderRegistry;

class AssetRepo : public System {
public:
  template<typename AssetType>
  static std::unique_ptr<AssetType> createAsset(AssetInfo&& info) {
    //TODO: is this a reliable way to set type? It seems easy to miss
    info.mType = Asset::typeId<AssetType>();
    return std::make_unique<AssetType>(std::move(info));
  }

  AssetRepo(const SystemArgs& args, std::unique_ptr<IAssetLoaderRegistry> loaderRegistry);
  ~AssetRepo();

  //TODO: find a better way to make this available
  static AssetRepo* get() {
    return sSingleton;
  }

  //If uri is provided it will be loaded if it doesn't exist. If only id is provided, only an existing asset will be returned
  std::shared_ptr<Asset> getAsset(AssetInfo info);
  template<typename AssetType>
  std::shared_ptr<AssetType> getAsset(AssetInfo info) {
    return std::static_pointer_cast<AssetType>(getAsset(info));
  }
  void getAssetsByCategory(std::string_view category, std::vector<std::shared_ptr<Asset>>& assets) const;

  void reloadAsset(std::shared_ptr<Asset> asset);
  void setBasePath(const std::string& basePath);
  //Add an asset without going through an AssetLoader. Intended for assets that aren't in files like built in physics models
  void addAsset(std::shared_ptr<Asset> asset);
  void forEachAsset(const std::function<void(std::shared_ptr<Asset>)> callback);
  void removeAsset(AssetInfo info);

private:
  void _fillInfo(AssetInfo& info);
  void _assetLoaded(AssetLoadResult result, std::shared_ptr<Asset> asset, AssetLoader& loader);
  //Get or create a loader from the pool
  AssetLoader* _getLoader(const std::string& category);
  std::shared_ptr<Asset> _find(AssetInfo& info);
  void _queueLoad(std::shared_ptr<Asset> asset);

  static const size_t sMaxLoaders = 5;

  std::string mBasePath;
  std::unordered_map<size_t, std::shared_ptr<Asset>> mIdToAsset;
  mutable RWLock mAssetLock;
  ThreadLocal<std::unordered_map<std::string, std::unique_ptr<AssetLoader>>> mLoaderPool;
  std::unique_ptr<IAssetLoaderRegistry> mLoaderRegistry;
  //TODO: find a better way to make this available
  static AssetRepo* sSingleton;
};

class IAssetLoaderRegistry {
public:
  using LoaderConstructor = std::function<std::unique_ptr<AssetLoader>(FileSystem::IFileSystem&)>;
  using AssetConstructor = std::function<std::shared_ptr<Asset>(AssetInfo&&)>;

  virtual ~IAssetLoaderRegistry() = default;
  virtual void registerLoader(const std::string& category, LoaderConstructor constructLoader, AssetConstructor constructAsset) = 0;
  virtual std::unique_ptr<AssetLoader> getLoader(FileSystem::IFileSystem& fileSystem, const std::string& category) = 0;
  virtual std::shared_ptr<Asset> getAsset(AssetInfo&& info) = 0;

  //Use this registration function if your AssetLoader/Asset constructors are default
  template<typename AssetType, typename LoaderType>
  void registerLoader(const std::string& category) {
    registerLoader(category, [category](FileSystem::IFileSystem& fs) {
      return std::make_unique<LoaderType>(fs, category);
    }, [](AssetInfo&& info) {
      return AssetRepo::createAsset<AssetType>(std::move(info));
    });
  }
};

namespace Registry {
  std::unique_ptr<IAssetLoaderRegistry> createAssetLoaderRegistry();
}