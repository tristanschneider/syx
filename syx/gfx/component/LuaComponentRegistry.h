#pragma once

class Component;
struct ComponentTypeInfo;

class LuaComponentRegistry {
public:
  using Constructor = std::function<std::unique_ptr<Component>(Handle)>;

  void registerComponent(const std::string& name, Constructor constructor);
  std::unique_ptr<Component> construct(const std::string& name, Handle owner) const;
  bool getComponentType(const std::string& name, size_t& type) const;
  const Component* getInstanceByPropName(const char* name) const;
  const Component* getInstanceByPropNameConstHash(size_t hash) const;

private:
  struct CompInfo {
    Constructor constructor;
    std::unique_ptr<Component> instance;
  };

  std::unordered_map<std::string, CompInfo> mNameToInfo;
  //Hash of Prop name (renderable) instead of type name (Renderable) pointing at CompInfo in mNameToInfo
  std::unordered_map<size_t, CompInfo*> mPropNameToInfo;
};