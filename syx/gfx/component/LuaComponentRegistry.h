#pragma once

class Component;
struct ComponentTypeInfo;

class LuaComponentRegistry {
public:
  using Constructor = std::function<std::unique_ptr<Component>(Handle)>;

  void registerComponent(const std::string& name, Constructor constructor);
  std::unique_ptr<Component> construct(const std::string& name, Handle owner) const;
  bool getComponentType(const std::string& name, size_t& type) const;

private:
  struct CompInfo {
    Constructor constructor;
    std::unique_ptr<Component> instance;
  };

  std::unordered_map<std::string, CompInfo> mNameToInfo;
};