#pragma once

struct AppTaskArgs;

namespace Loader {
  struct AssetLoadTask;
  struct LoadRequest;

  //Single-use reader for a given request
  class IAssimpReader {
  public:
    virtual ~IAssimpReader() = default;
    virtual bool isSceneExtension(std::string_view extension) = 0;
    virtual void loadScene(const Loader::LoadRequest& request) = 0;
  };

  std::unique_ptr<IAssimpReader> createAssimpReader(AssetLoadTask&, const AppTaskArgs&);
}