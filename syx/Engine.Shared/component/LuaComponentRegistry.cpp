#include "Precompile.h"
#include "component/LuaComponentRegistry.h"

#include "component/Component.h"
#include "Util.h"

namespace {
  class LuaComponentRegistry : public IComponentRegistry {
  public:
   void registerComponent(const std::string& name, IComponentRegistry::Constructor constructor) override {
      auto& info = mNameToInfo[name] = CompInfo{ constructor, constructor(0) };
      mPropNameToInfo[info.instance->getTypeInfo().mPropNameConstHash] = &info;
    }

    std::unique_ptr<Component> construct(const std::string& name, Handle owner) const override {
      auto it = mNameToInfo.find(name);
      if(it != mNameToInfo.end())
        return std::move(it->second.constructor(owner));
      return nullptr;
    }

    std::unique_ptr<Component> construct(const ComponentType& type, Handle owner) const override {
      auto found = std::find_if(mNameToInfo.begin(), mNameToInfo.end(), [&type](const auto& pair) {
        return pair.second.instance->getFullType() == type;
      });
      return found != mNameToInfo.end() ? found->second.constructor(owner) : nullptr;
    }

    std::optional<size_t> getComponentType(const std::string& name) const override {
      auto it = mNameToInfo.find(name);
      return it == mNameToInfo.end() ? std::nullopt : std::make_optional(it->second.instance->getType());
    }

    std::optional<ComponentType> getComponentFullType(const std::string& name) const override {
      auto it = mNameToInfo.find(name);
      return it == mNameToInfo.end() ? std::nullopt : std::make_optional(it->second.instance->getFullType());
    }

    const Component* getInstanceByPropName(const char* name) const override {
      return getInstanceByPropNameConstHash(Util::constHash(name));
    }

    const Component* getInstanceByPropNameConstHash(size_t hash) const override {
      auto it = mPropNameToInfo.find(hash);
      return it != mPropNameToInfo.end() ? it->second->instance.get() : nullptr;
    }

    void forEachComponent(const std::function<void(const Component&)>& callback) const override {
      for(const auto& it : mNameToInfo) {
        callback(*it.second.instance);
      }
    }

  private:
    struct CompInfo {
      Constructor constructor;
      std::unique_ptr<Component> instance;
    };

    std::unordered_map<std::string, CompInfo> mNameToInfo;
    //Hash of Prop name (renderable) instead of type name (Renderable) pointing at CompInfo in mNameToInfo
    std::unordered_map<size_t, CompInfo*> mPropNameToInfo;
  };
}

#include "provider/ComponentRegistryProvider.h"

namespace Registry {
  std::unique_ptr<IComponentRegistry> createComponentRegistry() {
    return std::make_unique<LuaComponentRegistry>();
  }

  std::unique_ptr<ComponentRegistryProvider> createComponentRegistryProvider() {
    struct Provider : public ComponentRegistryProvider {
      std::pair<const IComponentRegistry&, std::shared_lock<std::shared_mutex>> getReader() const override {
        const IComponentRegistry& reg = mRegistry;
        return std::make_pair(std::ref(reg), std::shared_lock<std::shared_mutex>(mMutex));
      }

      std::pair<IComponentRegistry&, std::unique_lock<std::shared_mutex>> getWriter() override {
        IComponentRegistry& reg = mRegistry;
        return std::make_pair(std::ref(reg), std::unique_lock<std::shared_mutex>(mMutex));
      }

      mutable std::shared_mutex mMutex;
      LuaComponentRegistry mRegistry;
    };

    return std::make_unique<Provider>();
  }
}