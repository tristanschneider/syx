#pragma once

class Component;

class LuaComponentRegistry {
public:
  using Constructor = std::function<std::unique_ptr<Component>(Handle)>;

  void registerComponent(const std::string& name, Constructor constructor);
  std::unique_ptr<Component> construct(const std::string& name, Handle owner) const;

private:
  std::unordered_map<std::string, Constructor> mNameToCtor;
};