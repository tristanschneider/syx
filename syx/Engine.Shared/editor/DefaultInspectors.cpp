#include "Precompile.h"
#include "DefaultInspectors.h"

#include "editor/InspectorFactory.h"
#include "lua/LuaNode.h"
#include "lua/LuaVariant.h"

namespace {
  template<class Type, class Container, class Func>
  void _registerFactory(Container& container, Func func) {
    container[typeId<Type>()] = [f = std::move(func)](const char* node, void* data) {
      return f(node, *reinterpret_cast<Type*>(data));
    };
  }
}

DefaultInspectors::DefaultInspectors() {
  _registerTypes();
}

bool DefaultInspectors::hasDefault(const Lua::Node& node) const {
  return getFactory(node) != nullptr;
}

bool DefaultInspectors::createInspector(const Lua::Node& node, void* data) const {
  if(auto func = getFactory(node))
    return (*func)(node.getName().c_str(), data);
  return false;
}

const DefaultInspectors::FactoryFunc* DefaultInspectors::getFactory(const Lua::Node& node) const {
  auto it = mFactory.find(node.getTypeId());
  if(it != mFactory.end())
    return &it->second;
  return nullptr;
}

void DefaultInspectors::_registerTypes() {
  using namespace Inspector;
  _registerFactory<std::string>(mFactory, inspectString);
  _registerFactory<bool>(mFactory, inspectBool);
  _registerFactory<Syx::Vec3>(mFactory, inspectVec3);
  _registerFactory<Syx::Mat4>(mFactory, inspectMat4);
  _registerFactory<int>(mFactory, inspectInt);
  _registerFactory<float>(mFactory, inspectFloat);
  _registerFactory<double>(mFactory, inspectDouble);
  _registerFactory<size_t>(mFactory, inspectSizeT);
  _registerFactory<Lua::Variant>(mFactory, inspectLuaVariant);
}
