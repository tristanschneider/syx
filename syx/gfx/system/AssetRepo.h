#pragma once
//Repository that manages acquisition and async loading of assets.
//AssetLoaders register themselves through AssetRepo::Loaders::registerLoader
//getAsset always returns an asset. This is either a previously loaded asset, or a
//newly created empty asset that will be soon loaded with the given loader in a task.
//Loaders are pooled so resources can be re-used in the same loader between loading of different assets.

#include "system/System.h"

class IWorkerPool;
class Asset;
class AssetLoader;
struct AssetInfo;
enum class AssetLoadResult : uint8_t;

class AssetRepo : public System {
public:
  RegisterSystemH(AssetRepo);
  class Loaders {
  public:
    using LoaderConstructor = std::function<std::unique_ptr<AssetLoader>()>;
    using AssetConstructor = std::function<std::shared_ptr<Asset>(AssetInfo&&)>;

    static void registerLoader(const std::string& category, LoaderConstructor constructLoader, AssetConstructor constructAsset);
    static std::unique_ptr<AssetLoader> getLoader(const std::string& category);
    static std::shared_ptr<Asset> getAsset(AssetInfo&& info);

  private:
    Loaders();
    ~Loaders();
    static Loaders& _get();

    std::unordered_map<std::string, std::pair<LoaderConstructor, AssetConstructor>> mCategoryToConstructors;
  };

  template<typename AssetType>
  static std::unique_ptr<AssetType> createAsset(AssetInfo&& info) {
    //TODO: is this a reliable way to set type? It seems easy to miss
    info.mType = Asset::typeId<AssetType>();
    return std::make_unique<AssetType>(std::move(info));
  }

  //Use this registration function if your AssetLoader/Asset constructors are default
  template<typename AssetType, typename LoaderType>
  static void registerLoader(const std::string& category) {
    Loaders::registerLoader(category, [category]() {
      return std::make_unique<LoaderType>(category);
    }, [](AssetInfo&& info) {
      return createAsset<AssetType>(std::move(info));
    });
  }

  //Don't use this directly, use the RegisterAssetLoader macro to statically register an asset loader
  //Makes it possible to statically register a loader instead of needing function scope
  template<typename AssetType, typename LoaderType>
  struct StaticRegisterLoader {
    StaticRegisterLoader(const std::string& category) {
      registerLoader<AssetType, LoaderType>(category);
    }
  };

  AssetRepo(const SystemArgs& args);
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

private:
  void _fillInfo(AssetInfo& info);
  void _assetLoaded(AssetLoadResult result, Asset& asset, AssetLoader& loader);
  //Get or create a loader from the pool
  AssetLoader* _getLoader(const std::string& category);
  std::shared_ptr<Asset> _find(AssetInfo& info);
  void _queueLoad(std::shared_ptr<Asset> asset);

  static const size_t sMaxLoaders = 5;

  std::string mBasePath;
  std::unordered_map<size_t, std::shared_ptr<Asset>> mIdToAsset;
  mutable RWLock mAssetLock;
  ThreadLocal<std::unordered_map<std::string, std::unique_ptr<AssetLoader>>> mLoaderPool;
  //TODO: find a better way to make this available
  static AssetRepo* sSingleton;
};

//Statically registers a loader for use in AssetRepo
//example usage RegisterAssetLoader("txt", TextAssetLoader, TextAsset)
#define RegisterAssetLoader(category, loaderType, assetType)\
namespace { AssetRepo::StaticRegisterLoader<assetType, loaderType> registeredLoader##loaderType(category); }
