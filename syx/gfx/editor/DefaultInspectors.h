#pragma once

namespace Lua {
  class Node;
}

class DefaultInspectors {
public:
  using FactoryFunc = std::function<bool(const Lua::Node&, void*)>;

  DefaultInspectors();

  bool hasDefault(const Lua::Node& node) const;
  bool createInspector(const Lua::Node& node, void* data) const;
  const FactoryFunc* getFactory(const Lua::Node& node) const;

private:
  void _registerTypes();

  std::unordered_map<size_t, FactoryFunc> mFactory;
};