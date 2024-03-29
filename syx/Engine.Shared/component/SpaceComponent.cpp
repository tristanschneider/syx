#include "Precompile.h"
#include "component/SpaceComponent.h"

#include "asset/Asset.h"
#include "component/LuaComponent.h"
#include "event/AssetEvents.h"
#include "event/BaseComponentEvents.h"
#include "event/EventBuffer.h"
#include "event/SpaceEvents.h"
#include "file/FilePath.h"
#include "file/FileSystem.h"
#include "lua/LuaCache.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaSerializer.h"
#include "lua/LuaStackAssert.h"
#include "lua/LuaState.h"
#include "lua/LuaUtil.h"
#include "LuaGameObject.h"
#include "lua/LuaNode.h"
#include "lua/LuaUtil.h"
#include <lua.hpp>
#include "ProjectLocator.h"
#include "provider/GameObjectHandleProvider.h"
#include "provider/MessageQueueProvider.h"
#include "registry/IDRegistry.h"
#include "Space.h"
#include "system/LuaGameSystem.h"
#include "threading/AsyncHandle.h"
#include "threading/FunctionTask.h"
#include "threading/WorkerPool.h"
#include "util/Finally.h"

namespace {
  void _pushComponent(MessageQueue& msg, Handle objHandle, const Component& comp) {
    msg.get().push(AddComponentEvent(objHandle, comp.getType(), comp.getSubType()));

    if(const Lua::Node* props = comp.getLuaProps()) {
      std::vector<uint8_t> buffer(props->size());
      props->copyConstructToBuffer(&comp, buffer.data());
      msg.get().push(SetComponentPropsEvent(objHandle, comp.getFullType(), props, ~Lua::NodeDiff(0), std::move(buffer)));
    }
  }

  void _pushObjectFromDescription(MessageQueueProvider& msgProvider, const LuaGameObjectDescription& obj, Handle space, IIDRegistry& idRegistry, bool fireAddEvent = true) {
    MessageQueue msg = msgProvider.getMessageQueue();
    //Create object
    if(fireAddEvent) {
      //TODO: how is this supposed to work if you're trying to load the same same scene into two different spaces?
      auto claimedID = idRegistry.tryClaimKnownID(obj.mUniqueID);
      //Generate a new key if the desired one is taken
      //TODO: once object relationships exist, a mapping would need to be updated here to preserve relationships
      if(!claimedID) {
        claimedID = idRegistry.generateNewUniqueID();
        printf("Remapped object with duplicate id %s to %s", std::to_string(obj.mUniqueID).c_str(), std::to_string(**claimedID).c_str());
      }
      msg.get().push(AddGameObjectEvent(obj.mRuntimeID, std::move(claimedID)));
    }

    //Copy components
    for(const auto& comp : obj.mComponents) {
      _pushComponent(msg, obj.mRuntimeID, *comp);
    }
    SpaceComponent spaceComp(0);
    spaceComp.set(space);
    _pushComponent(msg, obj.mRuntimeID, spaceComp);
  }
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
  Lua::checkGameContext(l).getOrCreateSpace(Util::constHash(name)).push(l);
  return 1;
}

const SpaceComponent& SpaceComponent::getObj(lua_State* l, int index) {
  return _checkSelf(l, "Space", index).get<SpaceComponent>();
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
  const SpaceComponent& from = getObj(l, 1);
  const SpaceComponent& to = getObj(l, 2);
  ILuaGameContext& game = Lua::checkGameContext(l);

  game.getMessageProvider().getMessageQueue().get().push(ClearSpaceEvent(to.get()));
  _addObjectsFromSpace(game, from.get(), to.get());
  return 0;
}

std::shared_ptr<IAsyncHandle<bool>> SpaceComponent::_save(lua_State* l, Handle space, const char* filename) {
  ILuaGameContext& game = Lua::checkGameContext(l);
  auto scene = std::make_shared<LuaSceneDescription>();
  scene->mName = FilePath(filename).getFileNameWithoutExtension();
  scene->mObjects.reserve(game.getObjects().size());

  auto result = Async::createAsyncHandle<bool>();

  for(const auto& obj : game.getObjects()) {
    const LuaGameObject& o = *obj.second;
    //Skip objects not in this space
    if(o.getSpace() != space)
      continue;

    LuaGameObjectDescription desc;
    //Runtime ID won't be saved, but set it anyway to avoid potential confusion
    desc.mRuntimeID = o.getHandle();
    desc.mUniqueID = o.getUniqueID();
    o.forEachComponent([&o, &desc](const Component& c) {
      desc.mComponents.emplace_back(std::move(c.clone()));
    });
    scene->mObjects.emplace_back(std::move(desc));
  }

  //No query means all assets
  //TODO: only save the assets that are used by this collection of objects
  game.getMessageProvider().getMessageQueue()->push(AssetQueryRequest({}).then(::typeId<LuaGameSystem, System>(),
    [file(std::string(filename)), scene, result, &game](const AssetQueryResponse& e) {
    scene->mAssets.reserve(e.mResults.size());
    for(const auto& asset : e.mResults) {
      scene->mAssets.emplace_back(asset->getInfo().mUri);
    }

    auto l = game.createLuaState();
    Lua::StackAssert sa(*l);
    scene->getMetadata().writeToLua(*l, scene.get(), Lua::Node::SourceType::FromGlobal);
    std::string serialized;
    Lua::Util::getDefaultSerializer().serializeGlobal(*l, scene->ROOT_KEY, serialized);
    //Remove scene from global
    lua_pushnil(*l);
    lua_setglobal(*l, scene->ROOT_KEY);

    Async::setComplete(*result, game.getFileSystem().writeFile(file.c_str(), serialized) == FileSystem::FileResult::Success);
  }));

  return result;
}

int SpaceComponent::save(lua_State* l) {
  const SpaceComponent& self = getObj(l, 1);
  const char* name = luaL_checkstring(l, 2);
  _save(l, self.get(), _sceneNameToFullPath(Lua::checkGameContext(l), name));
  return 0;
}

std::shared_ptr<IAsyncHandle<bool>> SpaceComponent::_load(lua_State* l, Handle space, const char* filename) {
  ILuaGameContext& game = Lua::checkGameContext(l);
  const FilePath path(filename);
  if(!game.getFileSystem().fileExists(path)) {
    return nullptr;
  }

  auto result = Async::createAsyncHandle<bool>();
  game.getWorkerPool().queueTask(std::make_shared<FunctionTask>([&game, path, space, result]() {
    auto s = game.createLuaState();

    Lua::StackAssert sa(*s);
    std::vector<uint8_t> serializedScene;
    auto completeOnFailure(finally([&result] {
      Async::setComplete(*result, false);
    }));
    if(game.getFileSystem().readFile(path, serializedScene) == FileSystem::FileResult::Success) {
      if(luaL_dostring(*s, reinterpret_cast<const char*>(serializedScene.data())) == LUA_OK) {
        LuaSceneDescription sceneDesc;
        sceneDesc.getMetadata().readFromLua(*s, &sceneDesc, Lua::Node::SourceType::FromGlobal);
        _loadSceneFromDescription(game, sceneDesc, space);
        //Assume success if lua was properly formatted
        Async::setComplete(*result, true);
        completeOnFailure.cancel();
      }
      else {
        printf("Error loading scene %s\n", lua_tostring(*s, -1));
        lua_pop(*s, 1);
      }
    }
  }));
  return result;
}

int SpaceComponent::load(lua_State* l) {
  const SpaceComponent& self = getObj(l, 1);
  const char* name = luaL_checkstring(l, 2);

  const bool exists = _load(l, self.get(), _sceneNameToFullPath(Lua::checkGameContext(l), name)) != nullptr;

  lua_pushboolean(l, static_cast<int>(exists));
  return 1;
}

void SpaceComponent::_loadSceneFromDescription(ILuaGameContext& game, LuaSceneDescription& scene, Handle space) {
  //TODO: should this be here? It causes issues with play/pause since the scene is lost after timescale is set
  //game.getMessageQueue().get().push(ClearSpaceEvent(space));
  SpaceComponent destSpaceComp(0);
  destSpaceComp.set(space);
  //Cause loading of all necessary assets
  for(const std::string& asset : scene.mAssets) {
    game.getMessageProvider().getMessageQueue()->push(GetAssetRequest(AssetInfo(asset)));
  }
  GameObjectHandleProvider& objGen = game.getGameObjectGen();
  for(LuaGameObjectDescription& obj : scene.mObjects) {
    obj.mRuntimeID = objGen.newHandle();
    _pushObjectFromDescription(game.getMessageProvider(), obj, space, game.getIDRegistry());
  }
}

void SpaceComponent::_addObjectsFromSpace(ILuaGameContext& game, Handle fromSpace, Handle toScene) {
  MessageQueue msg = game.getMessageProvider().getMessageQueue();
  SpaceComponent newSpaceComp(0);
  newSpaceComp.set(toScene);

  for(const auto& it : game.getObjects()) {
    //Create object
    const LuaGameObject& obj = *it.second;
    if(obj.getSpace() != fromSpace)
      continue;
    const Handle objHandle = obj.getHandle();
    msg.get().push(AddGameObjectEvent(objHandle));

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
  const SpaceComponent& self = getObj(l, 1);
  Lua::checkGameContext(l).getMessageProvider().getMessageQueue().get().push(ClearSpaceEvent(self.get()));
  return 0;
}

int SpaceComponent::addObject(lua_State* l) {
  const SpaceComponent& self = getObj(l, 1);
  luaL_checktype(l, 2, LUA_TTABLE);
  ILuaGameContext& game = Lua::checkGameContext(l);

  Lua::StackAssert sa(l, 1);
  lua_pushvalue(l, -1);
  LuaGameObjectDescription desc;
  desc.getMetadata().readFromLua(l, &desc, Lua::Node::SourceType::FromStack);
  IGameObject& obj = game.addGameObject();
  desc.mRuntimeID = obj.getRuntimeID();
  desc.mUniqueID = obj.getUniqueID();
  _pushObjectFromDescription(game.getMessageProvider(), desc, self.get(), game.getIDRegistry(), false);
  lua_pop(l, 1);

  LuaGameObject::push(l, obj);
  return 1;
}

FilePath SpaceComponent::_sceneNameToFullPath(ILuaGameContext& game, const char* scene) {
  FilePath path = game.getProjectLocator().transform(scene, PathSpace::Project, PathSpace::Full);
  return path.addExtension(LuaSceneDescription::FILE_EXTENSION);
}
