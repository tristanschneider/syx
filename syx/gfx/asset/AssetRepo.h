#pragma once

class IWorkerPool;
class Asset;
class AssetLoader;
struct AssetInfo;
enum class AssetLoadResult : uint8_t;

class AssetRepo {
public:
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

  AssetRepo(const std::string& basePath, IWorkerPool& pool);

  std::shared_ptr<Asset> getAsset(AssetInfo info);

private:
  void _fillInfo(AssetInfo& info);
  void _assetLoaded(AssetLoadResult result, Asset& asset);
  //Get or create a loader from the pool
  std::unique_ptr<AssetLoader> _getLoader(const std::string& category);
  //Return a loader to the pool
  void _returnLoader(std::unique_ptr<AssetLoader> loader);

  static const size_t sMaxLoaders = 5;

  std::string mBasePath;
  IWorkerPool& mPool;
  std::unordered_map<size_t, std::shared_ptr<Asset>> mIdToAsset;
  std::mutex mAssetMutex;
  std::unordered_map<std::string, std::vector<std::unique_ptr<AssetLoader>>> mLoaderPool;
  std::mutex mLoaderMutex;
};