#pragma once
#include "Component.h"

namespace Lua {
  class Cache;
};

class FilePath;
struct lua_State;
struct ILuaGameContext;
struct LuaSceneDescription;

template<class ResultT>
struct IAsyncHandle;

class SpaceComponent : public TypedComponent<SpaceComponent> {
public:
  using TypedComponent::TypedComponent;

  void set(Handle id);
  Handle get() const;

  std::unique_ptr<Component> clone() const override;
  void set(const Component& component) override;

  virtual const Lua::Node* getLuaProps() const override;

  COMPONENT_LUA_INHERIT(SpaceComponent);
  virtual void openLib(lua_State* l) const override;
  virtual const ComponentTypeInfo& getTypeInfo() const override;

  // Space get(string name)
  static int get(lua_State* l);
  //void cloneTo(self, Scene to)
  static int cloneTo(lua_State* l);
  //void save(self, string filename)
  static int save(lua_State* l);
  static std::shared_ptr<IAsyncHandle<bool>> _save(lua_State* l, Handle space, const char* filename);
  //returns true if scene exists
  //bool load(self, string filename)
  static int load(lua_State* l);
  //Can return nullptr if scene doesn't exist, async bool indicates success of the async load
  static std::shared_ptr<IAsyncHandle<bool>> _load(lua_State* l, Handle space, const char* filename);
  //void clear(self)
  static int clear(lua_State* l);
  //Gameobject addObject(self, table)
  static int addObject(lua_State* l);

  static const SpaceComponent& getObj(lua_State* l, int index);
  static void _loadSceneFromDescription(ILuaGameContext& game, LuaSceneDescription& scene, Handle space);
  static void _addObjectsFromSpace(ILuaGameContext& game, Handle fromSpace, Handle toSpace);

private:
  static FilePath _sceneNameToFullPath(ILuaGameContext& game, const char* scene);
  std::unique_ptr<Lua::Node> _buildLuaProps() const;

  Handle mId = 0;
};