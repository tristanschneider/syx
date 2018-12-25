#include "Precompile.h"
#include "component/SpaceComponent.h"

#include "component/LuaComponent.h"
#include "event/BaseComponentEvents.h"
#include "event/EventBuffer.h"
#include "event/SpaceEvents.h"
#include "file/FilePath.h"
#include "file/FileSystem.h"
#include "lua/LuaCache.h"
#include "lua/LuaSerializer.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include "lua/LuaUtil.h"
#include "LuaGameObject.h"
#include "lua/LuaNode.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>
#include "ProjectLocator.h"
#include "provider/MessageQueueProvider.h"
#include "system/LuaGameSystem.h"
#include "threading/FunctionTask.h"
#include "threading/WorkerPool.h"

namespace {
  void _pushComponent(MessageQueue& msg, Handle objHandle, const Component& comp) {
    msg.get().push(AddComponentEvent(objHandle, comp.getType(), comp.getSubType()));

    if(const Lua::Node* props = comp.getLuaProps()) {
      std::vector<uint8_t> buffer(props->size());
      props->copyConstructToBuffer(&comp, buffer.data());
      msg.get().push(SetComponentPropsEvent(objHandle, comp.getType(), props, ~0, std::move(buffer), comp.getSubType()));
    }
  }

  void _pushObjectFromDescription(LuaGameSystem& game, const LuaGameObjectDescription& obj, Handle space, bool fireAddEvent = true) {
    MessageQueue msg = game.getMessageQueue();
    //Create object
    if(fireAddEvent)
      msg.get().push(AddGameObjectEvent(obj.mHandle));

    //Copy components
    for(const auto& comp : obj.mComponents) {
      _pushComponent(msg, obj.mHandle, *comp);
    }
    SpaceComponent spaceComp(0);
    spaceComp.set(space);
    _pushComponent(msg, obj.mHandle, spaceComp);
  }
}

DEFINE_COMPONENT(SpaceComponent)
  , mId(0) {
}

SpaceComponent::SpaceComponent(const SpaceComponent& rhs)
  : Component(rhs.getType(), rhs.getOwner())
  , mId(rhs.mId) {
}

void SpaceComponent::set(Handle id) {
  mId = id;
}

Handle SpaceComponent::get() const {
  return mId;
}

std::unique_ptr<Component> SpaceComponent::clone() const {
  return std::make_unique<SpaceComponent>(*this);
}

void SpaceComponent::set(const Component& component) {
  mId = static_cast<const SpaceComponent&>(component).mId;
}

void SpaceComponent::openLib(lua_State* l) const {
  luaL_Reg statics[] = {
    { "get", get },
    { nullptr, nullptr }
  };
  luaL_Reg members[] = {
    COMPONENT_LUA_BASE_REGS,
    { "cloneTo", cloneTo },
    { "save", save },
    { "load", load },
    { "clear", clear },
    { "addObject", addObject },
    { nullptr, nullptr }
  };
  Lua::Util::registerClass(l, statics, members, getTypeInfo().mTypeName.c_str());
}

const ComponentTypeInfo& SpaceComponent::getTypeInfo() const {
  static ComponentTypeInfo info("Space");
  return info;
}

int SpaceComponent::get(lua_State* l) {
  const char* name = luaL_checkstring(l, 1);
  LuaGameSystem& game = LuaGameSystem::check(l);
  game.getSpace(Util::constHash(name)).push(l);
  return 1;
}

SpaceComponent& SpaceComponent::getObj(lua_State* l, int index) {
  return *static_cast<SpaceComponent*>(getLuaCache().checkParam(l, index, "Space"));
}

const Lua::Node* SpaceComponent::getLuaProps() const {
  static std::unique_ptr<Lua::Node> props = _buildLuaProps();
  return props.get();
}

std::unique_ptr<Lua::Node> SpaceComponent::_buildLuaProps() const {
  using namespace Lua;
  auto root = makeRootNode(NodeOps(""));
  makeNode<SizetNode>(NodeOps(*root, "id", ::Util::offsetOf(*this, mId)));
  return std::move(root);
}

int SpaceComponent::cloneTo(lua_State* l) {
  SpaceComponent& from = getObj(l, 1);
  SpaceComponent& to = getObj(l, 2);
  LuaGameSystem& game = LuaGameSystem::check(l);

  game.getMessageQueue().get().push(ClearSpaceEvent(to.get()));
  _addObjectsFromSpace(game, from.get(), to.get());
  return 0;
}

void SpaceComponent::_save(lua_State* l, Handle space, const char* filename) {
  LuaGameSystem& game = LuaGameSystem::check(l);
  LuaSceneDescription scene;
  scene.mName = filename;
  scene.mObjects.reserve(game.getObjects().size());

  for(const auto& obj : game.getObjects()) {
    const LuaGameObject& o = *obj.second;
    //Skip objects not in this space
    if(o.getSpace() != space)
      continue;

    LuaGameObjectDescription desc;
    desc.mHandle = o.getHandle();
    o.forEachComponent([&o, &desc](const Component& c) {
      desc.mComponents.emplace_back(std::move(c.clone()));
    });
    scene.mObjects.emplace_back(std::move(desc));
  }

  Lua::StackAssert sa(l);
  scene.getMetadata().writeToLua(l, &scene, Lua::Node::SourceType::FromGlobal);
  std::string serialized;
  Lua::Util::getDefaultSerializer().serializeGlobal(l, scene.ROOT_KEY, serialized);
  //Remove scene from global
  lua_pushnil(l);
  lua_setglobal(l, scene.ROOT_KEY);

  FilePath path = _sceneNameToFullPath(game, filename);
  FileSystem::writeFile(path, serialized);
}

int SpaceComponent::save(lua_State* l) {
  SpaceComponent& self = getObj(l, 1);
  const char* name = luaL_checkstring(l, 2);
  _save(l, self.get(), name);
  return 0;
}

int SpaceComponent::load(lua_State* l) {
  SpaceComponent& self = getObj(l, 1);
  LuaGameSystem& game = LuaGameSystem::check(l);
  const char* name = luaL_checkstring(l, 2);

  FilePath path = self._sceneNameToFullPath(game, name);
  bool exists = FileSystem::fileExists(path);
  game.getWorkerPool().queueTask(std::make_shared<FunctionTask>([&game, path, &self]() {
    Lua::State s;
    game._openAllLibs(s);

    Lua::StackAssert sa(s);
    std::string serializedScene;
    if(FileSystem::readFile(path, serializedScene) == FileSystem::FileResult::Success) {
      if(luaL_dostring(s, serializedScene.c_str()) == LUA_OK) {
        LuaSceneDescription sceneDesc;
        sceneDesc.getMetadata().readFromLua(s, &sceneDesc, Lua::Node::SourceType::FromGlobal);
        _loadSceneFromDescription(game, sceneDesc, self.get());
      }
      else {
        printf("Error loading scene %s\n", lua_tostring(s, -1));
        lua_pop(s, 1);
      }
    }
  }));
  return exists;
}

void SpaceComponent::_loadSceneFromDescription(LuaGameSystem& game, const LuaSceneDescription& scene, Handle space) {
  game.getMessageQueue().get().push(ClearSpaceEvent(space));
  SpaceComponent destSpaceComp(0);
  destSpaceComp.set(space);
  for(const LuaGameObjectDescription& obj : scene.mObjects) {
    _pushObjectFromDescription(game, obj, space);
  }
}

void SpaceComponent::_addObjectsFromSpace(LuaGameSystem& game, Handle fromSpace, Handle toScene) {
  MessageQueue msg = game.getMessageQueue();
  SpaceComponent newSpaceComp(0);
  newSpaceComp.set(toScene);

  for(const auto& it : game.getObjects()) {
    //Create object
    const LuaGameObject& obj = *it.second;
    if(obj.getSpace() != fromSpace)
      continue;
    msg.get().push(AddGameObjectEvent(obj.getHandle()));
    Handle objHandle = obj.getHandle();

    //Copy components
    obj.forEachComponent([&msg, objHandle](const Component& comp) {
      // Skip space, we'll set it explicitly at the end
      if(comp.getType() != Component::typeId<SpaceComponent>()) {
        _pushComponent(msg, objHandle, comp);
      }
    });

    //Set new space
    _pushComponent(msg, objHandle, newSpaceComp);
  }
}

int SpaceComponent::clear(lua_State* l) {
  SpaceComponent& self = getObj(l, 1);
  LuaGameSystem::check(l).getMessageQueue().get().push(ClearSpaceEvent(self.get()));
  return 0;
}

int SpaceComponent::addObject(lua_State* l) {
  SpaceComponent& self = getObj(l, 1);
  luaL_checktype(l, 2, LUA_TTABLE);
  LuaGameSystem& game = LuaGameSystem::check(l);

  Lua::StackAssert sa(l, 1);
  lua_pushvalue(l, -1);
  LuaGameObjectDescription desc;
  desc.getMetadata().readFromLua(l, &desc, Lua::Node::SourceType::FromStack);
  LuaGameObject& obj = game.addGameObject();
  desc.mHandle = obj.getHandle();
  _pushObjectFromDescription(game, desc, self.get(), false);
  lua_pop(l, 1);

  LuaGameObject::push(l, obj);
  return 1;
}

FilePath SpaceComponent::_sceneNameToFullPath(LuaGameSystem& game, const char* scene) {
  FilePath path = game.getProjectLocator().transform(scene, PathSpace::Project, PathSpace::Full);
  return path.addExtension(LuaSceneDescription::FILE_EXTENSION);
}
