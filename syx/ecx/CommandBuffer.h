#pragma once

#include "LinearEntityRegistry.h"
#include "View.h"

namespace ecx {
  //Command buffer for deferred creation/destruction of components and entities
  //Mixing the timing of adding an entity with adding a component is not allowed
  //as it would make the buffer processing more complicated than necessary
  template<class EntityT>
  class CommandBuffer {
  };

  template<>
  class CommandBuffer<LinearEntity> {
  public:
    using IDGenerator = std::function<LinearEntity()>;
    using CommandEntity = uint64_t;
    using CommandRegistry = EntityRegistry<CommandEntity>;
    template<class... Args>
    using CommandView = View<CommandEntity, Args...>;
    using DestinationRegistry = EntityRegistry<LinearEntity>;

    CommandBuffer() = default;
    CommandBuffer(CommandBuffer&&) = default;

    CommandBuffer(DestinationRegistry& registry)
      : mEntityGenerator(registry.createEntityGenerator()) {
    }
    CommandBuffer(std::shared_ptr<IndependentEntityGenerator> gen)
      : mEntityGenerator(std::move(gen)) {
    }

    template<class... Components>
    auto createAndGetEntityWithComponents() {
      LinearEntity entity = _createCommandEntity(EntityCommandType::CreateAndAddComponents);
      CreateCommandChunkID chunk;
      chunk.mChunkID = LinearEntity::buildChunkId<Components...>();
      chunk.mCreateChunk = [](DestinationRegistry& registry) {
        return registry.getOrCreateChunk<Components...>();
      };
      mCommandEntities.addComponent<CreateCommandChunkID>(entity.mData.mRawId, std::move(chunk));
      mCommandEntities.addComponent<EntityAddRemoveFilterTag>(entity.mData.mRawId);
      //These are all `AssignComponent` cases because the entity command will add it to the chunk which will default construct all component types,
      //then they will be filled in by assignments
      return std::make_tuple(entity, _addComponentCommand<Components>(entity.mData.mRawId, ComponentCommandType::AssignComponent)...);
    }

    void destroyEntity(const LinearEntity& entity) {
      //If this was an entity created in the command buffer, remove the command
      if(EntityCommandType* existingCommand = mCommandEntities.tryGetComponent<EntityCommandType>(entity.mData.mRawId)) {
        switch(*existingCommand) {
          case EntityCommandType::RemoveEntity:
            //If there's already a command for removal, don't need to do anything
            return;
          case EntityCommandType::CreateAndAddComponents:
            //If this was a fresh entity, then the command for it can be removed
            _setComponentCommandOnAllComponents(entity.mData.mRawId, ComponentCommandType::SkipCommand);
            *existingCommand = EntityCommandType::SkipCommand;
            return;
          case EntityCommandType::AddRemoveComponents:
            //If this command was for adding some components, erase the add/remove commands and replace with a remove entity command
            _setComponentCommandOnAllComponents(entity.mData.mRawId, ComponentCommandType::SkipCommand);
            *existingCommand = EntityCommandType::RemoveEntity;
            return;
            //Recycle processed command slot into a new remove command
          case EntityCommandType::SkipCommand:
            *existingCommand = EntityCommandType::RemoveEntity;
            break;
        }
      }
      else {
        //A command does not already exist for this, create it
        _createCommandEntity(entity, EntityCommandType::RemoveEntity);
      }
      mCommandEntities.addComponent<EntityAddRemoveFilterTag>(entity.mData.mRawId);
    }

    template<class Component>
    Component& addComponent(const LinearEntity& entity) {
      return *_addOrRemoveCommandComponent<Component>(entity, ComponentCommandType::AddComponent);
    }

    struct RuntimeComponentTypes {
      template<class ComponentT>
      static RuntimeComponentTypes create() {
        return { typeId<ComponentCommand<ComponentT>, CommandEntity>(), typeId<ComponentT, CommandEntity>(), typeId<ComponentT, LinearEntity>() };
      }

      typeId_t<CommandEntity> mCommandType;
      typeId_t<CommandEntity> mComponentType;
      typeId_t<LinearEntity> mDestinationType;
    };

    //TODO: need a better solution for this, this creates new tpye ids for every instance of the command buffer
    std::unordered_map<typeId_t<LinearEntity>, RuntimeComponentTypes> mTypeMap;

    RuntimeComponentTypes _convertTypeId(const typeId_t<LinearEntity>& type) {
      if(auto it = mTypeMap.find(type); it != mTypeMap.end()) {
        return it->second;
      }
      RuntimeComponentTypes types{ claimTypeId<CommandEntity>(), claimTypeId<CommandEntity>(), type };
      mTypeMap[type] = types;
      return types;
    }

    void* addRuntimeComponent(const LinearEntity& entity, const StorageInfo<LinearEntity>& info) {
      RuntimeComponentTypes types = _convertTypeId(info.mType);
      const StorageInfo<CommandEntity> storage = _toCommandStorage(info, types.mComponentType, *info.mComponentTraits);

      return _addOrRemoveCommandComponent(entity, ComponentCommandType::AddComponent, storage, types);
    }

    void removeRuntimeComponent(const LinearEntity& entity, const StorageInfo<LinearEntity>& info) {
      RuntimeComponentTypes types = _convertTypeId(info.mType);
      const StorageInfo<CommandEntity> storage = _toCommandStorage(info, types.mComponentType, *info.mComponentTraits);

      _addOrRemoveCommandComponent(entity, ComponentCommandType::RemoveComponent, storage, types);
    }

    template<class Component>
    Component* tryGetPendingComponent(const LinearEntity& entity) {
      return mCommandEntities.tryGetComponent<Component>(entity.mData.mRawId);
    }

    template<class Component>
    void removeComponent(const LinearEntity& entity) {
      _addOrRemoveCommandComponent<Component>(entity, ComponentCommandType::RemoveComponent);
    }

    void processAllCommands(EntityRegistry<LinearEntity>& registry) {
      EntityCommandView entityCommands(mCommandEntities);
      IndependentEntityGenerator& generator = *mEntityGenerator;
      //First ensure all necessary entities have been created or removed
      for(auto it = entityCommands.begin(); it != entityCommands.end(); ++it) {
        EntityCommandType& type = (*it).get<EntityCommandType>();
        _processEntityCommand(registry, type, it, generator);
      }

      //Process per-component commands of each pool
      for(auto pool = mCommandEntities.poolsBegin(); pool != mCommandEntities.poolsEnd(); ++pool) {
        //Processor will be empty for pools unrelated to the commands
        if(const ComponentCommandProcessor& processor = pool.getSharedComponents().emplace<ComponentCommandProcessor>(); processor.processCommands) {
          processor.processCommands(mCommandEntities, registry, processor.mTypes);
        }
      }
    }

    //Process all commands that this component type are involved in
    //This can include creation commands which affect other component types
    template<class Component>
    void processCommandsForComponent(DestinationRegistry& registry) {
      _processAllEntityCommands(mCommandEntities, registry, *mEntityGenerator);
      _processAllComponentCommands<Component>(mCommandEntities, registry);
    }

    void clear() {
      mCommandEntities.clear();
    }

  private:

    enum class EntityCommandType : uint8_t {
      CreateAndAddComponents,
      RemoveEntity,
      AddRemoveComponents,
      //This command has already been processed or is no longer needed, skip it
      //Skip is used instead of removal so swap remove doesn't disrupt the order
      //Instead, commands are cleared after `processAllCommands`
      SkipCommand,
    };

    enum class ComponentCommandType : uint8_t {
      AddComponent,
      RemoveComponent,
      AssignComponent,
      SkipCommand,
    };
    template<class ComponentT>
    struct ComponentCommand {
      ComponentCommandType mType{};
    };
    struct RuntimeTag {};
    using RuntimeComponentCommand = ComponentCommand<RuntimeTag>;

    //Tag for commands involving adding or removing entities
    struct EntityAddRemoveFilterTag {};
    //The id of the chunk that a new entity (CreateAndAddComponents) belongs to
    struct CreateCommandChunkID {
      using DestinationRegistry = EntityRegistry<LinearEntity>;
      uint32_t mChunkID = 0;
      std::function<DestinationRegistry::ChunkIterator(DestinationRegistry&)> mCreateChunk;
    };

    using EntityCommandView = CommandView<
      Include<EntityAddRemoveFilterTag>,
      Write<EntityCommandType>,
      OptionalRead<CreateCommandChunkID>>;

    struct ComponentCommandProcessor {
      void (*processCommands)(CommandRegistry&, DestinationRegistry&, const RuntimeComponentTypes&) = nullptr;
      void (*setComponentCommand)(CommandRegistry&, ComponentCommandType, const CommandEntity&, const RuntimeComponentTypes&) = nullptr;
      RuntimeComponentTypes mTypes;
    };

    template<class ComponentT, class EntityT>
    static StorageInfo<EntityT> _getStorageInfo() {
      return typename ComponentTraits<ComponentT>::getStorageInfo<EntityT>();
    }

    template<class Component>
    void _updateComponentCommandForAddRemove(const CommandEntity& entity, EntityCommandType& existingCommand, ComponentCommandType& type) {
      ComponentCommand<Component>* existingComponentCommand = mCommandEntities.tryGetComponent<ComponentCommand<Component>>(entity);
      _updateComponentCommandForAddRemove(entity, existingCommand, _getStorageInfo<Component, LinearEntity>(), existingComponentCommand ? &existingComponentCommand->mType : nullptr);
    }

    void _updateComponentCommandForAddRemove(const CommandEntity& entity, EntityCommandType& existingCommand, ComponentCommandType& type, const StorageInfo<LinearEntity>& info, ComponentCommandType* existingComponentCommand) {
      switch(existingCommand) {
      //If this is already an add/remove case then nothing special needed
      case EntityCommandType::AddRemoveComponents:
        break;
      //If this is a new entity, the chunk id needs to be updated then the value assigned
      case EntityCommandType::CreateAndAddComponents: {
        //Chunk needs to be updated if this is not a redundant command
        const bool updateChunkID = !existingComponentCommand || *existingComponentCommand != type;
        if(updateChunkID) {
          CreateCommandChunkID& chunk = mCommandEntities.getComponent<CreateCommandChunkID>(entity);
          chunk.mChunkID = LinearEntity::buildChunkId(chunk.mChunkID, info.mType);
          auto prev = chunk.mCreateChunk;
          chunk.mCreateChunk = [prev, type, info](DestinationRegistry& registry) {
            switch(type) {
              case ComponentCommandType::AssignComponent:
              case ComponentCommandType::SkipCommand:
                assert(false && "unexpected type");
                break;
              case ComponentCommandType::AddComponent: {
                auto prevChunkIt = prev(registry);
                return prevChunkIt.hasComponent(info.mType) ? prevChunkIt : registry.getOrCreateChunkAddedComponent(prevChunkIt, info);
              }
              case ComponentCommandType::RemoveComponent: {
                auto prevChunkIt = prev(registry);
                return prevChunkIt.hasComponent(info.mType) ? registry.getOrCreateChunkRemovedComponent(prev(registry), info) : prevChunkIt;
              }
            }
            return registry.endChunks();
          };
        }
        //Since chunk was updated above, component will be added upon creation, and needs to be assigned
        type = type == ComponentCommandType::RemoveComponent ? ComponentCommandType::RemoveComponent : ComponentCommandType::AssignComponent;
        break;
      }
      case EntityCommandType::RemoveEntity:
        if(type == ComponentCommandType::RemoveComponent) {
          //Calling remove component on a removed entity is fine I guess, otherwise fall through to assert
          break;
        }
        assert(false && "Invalid state");
        break;
      case EntityCommandType::SkipCommand:
        //Recycle skipped command into the new command
        existingCommand = EntityCommandType::AddRemoveComponents;
        break;
      }
    }

    void* _addOrRemoveCommandComponent(const LinearEntity& entity, ComponentCommandType type, const StorageInfo<CommandEntity>& storage, const RuntimeComponentTypes& types) {
      const StorageInfo<LinearEntity> destinationStorage = _toDestinationStorage(storage, types.mDestinationType);
      if(EntityCommandType* existingCommand = mCommandEntities.tryGetComponent<EntityCommandType>(entity.mData.mRawId)) {
        StorageInfo<CommandEntity> commandStorage = _getStorageInfo<RuntimeComponentCommand, CommandEntity>();
        commandStorage.mType = types.mCommandType;
        auto* componentCommand = static_cast<RuntimeComponentCommand*>(mCommandEntities.tryGetComponent(entity.mData.mRawId, commandStorage));
        _updateComponentCommandForAddRemove(entity.mData.mRawId, *existingCommand, type, destinationStorage, componentCommand ? &componentCommand->mType : nullptr);
      }
      else {
        _createCommandEntity(entity, EntityCommandType::AddRemoveComponents);
      }
      RuntimeComponentCommand componentCommand{ type };
      return _addComponentCommand(entity.mData.mRawId, type, destinationStorage,types, &componentCommand);
    }

    template<class Component>
    Component* _addOrRemoveCommandComponent(const LinearEntity& entity, ComponentCommandType type) {
      return static_cast<Component*>(_addOrRemoveCommandComponent(entity,type, _getStorageInfo<Component, CommandEntity>(), RuntimeComponentTypes::create<Component>()));
    }

    template<class Component>
    static void _processAllComponentCommands(CommandRegistry& commandEntities, DestinationRegistry& registry) {
      RuntimeComponentTypes types;
      types.mCommandType = typeId<ComponentCommand<Component>, CommandEntity>();
      types.mComponentType = typeId<Component, CommandEntity>();
      types.mDestinationType = typeId<Component, LinearEntity>();
      _processAllRuntimeComponentCommands(commandEntities, registry, types);
    }

    static void _processAllRuntimeComponentCommands(CommandRegistry& commandEntities, DestinationRegistry& registry, const RuntimeComponentTypes& types) {
      auto commandPool = commandEntities.findPool(types.mCommandType);
      auto componentPoolIt = commandEntities.findPool(types.mComponentType);
      CommandRegistry::PoolIt* componentPool = nullptr;
      StorageInfo<CommandEntity> componentStorage;
      if(componentPoolIt != commandEntities.poolsEnd()) {
        componentPool = &componentPoolIt;
        componentStorage = componentPool->getStorageInfo();
      }
      auto entityCommandPool = commandEntities.findPool<EntityCommandType>();
      //ComponentPool is optional since it's not needed for some command types
      if(commandPool == commandEntities.poolsEnd() || entityCommandPool == commandEntities.poolsEnd()) {
        return;
      }

      //Manual iteration since I don't have runtime views. Lead with the component command
      for(auto pair : commandPool.getEntities()) {
        const CommandEntity entity = pair.mSparseId;
        //This is assuming that the layout of the member for each instantiation of the class is the same
        ComponentCommandType componentCommandType = static_cast<RuntimeComponentCommand*>(commandPool.at(static_cast<size_t>(pair.mPackedId)))->mType;
        static_assert(sizeof(RuntimeComponentCommand) == sizeof(ComponentCommand<int>));

        switch(componentCommandType) {
          case ComponentCommandType::SkipCommand:
            continue;

          //Assign is from the creation of new entities default constructed in a given chunk
          //If the component doesn't exist it means the chunk creation didn't work properly
          case ComponentCommandType::AssignComponent:
            if(!componentPool) {
              continue;
            }
            if(void* destination = registry.tryGetComponent(entity, types.mDestinationType)) {
              if(void* source = componentPool->tryGetComponent(entity)) {
                componentStorage.mComponentTraits->moveAssign(source, destination);
              }
              else {
                assert(false && "Command component storage should exist if ComponentCommand was added for its type");
              }
            }
            else {
              assert(false && "New entity's chunk should result in having all the desired components");
            }
            break;

          case ComponentCommandType::AddComponent: {
            //AddRemove means the entity already exists, figure out if it's an addition or removal then make that change
            if(void* toAdd = componentPool->tryGetComponent(entity)) {
              registry.addRuntimeComponent(entity, _toDestinationStorage(componentStorage, types.mDestinationType), toAdd);
            }
            else {
              assert(false && "Command component storage should exist if ComponentCommand was added for its type");
            }
            break;

          case ComponentCommandType::RemoveComponent:
            registry.removeComponent(entity, types.mDestinationType);
            break;
          }
        }
      }

      //Remove all commands of this type now that they've been processed
      //Don't delete the entire entity yet as there may still be other components involved
      commandPool.clear();
    }

    static void _processAllEntityCommands(CommandRegistry& commandEntities, DestinationRegistry& registry, IndependentEntityGenerator& generator) {
      EntityCommandView entityCommands(commandEntities);
      //First ensure all necessary entities have been created or removed
      for(auto it = entityCommands.begin(); it != entityCommands.end(); ++it) {
        EntityCommandType& type = (*it).get<EntityCommandType>();
        _processEntityCommand(registry, type, it, generator);
      }
    }

    static void _processEntityCommand(DestinationRegistry& registry, EntityCommandType& type, EntityCommandView::It& commandIt, IndependentEntityGenerator& generator) {
      switch(type) {
        //If add/remove has already been processed, move on
        case EntityCommandType::SkipCommand:
          break;
        //This is an unprocessed remove entity command. Remove the entity then mark this command as processed
        case EntityCommandType::RemoveEntity:
          registry.destroyEntity((*commandIt).entity(), generator);
          type = EntityCommandType::SkipCommand;
          break;
        //This is an unprocessed add entity command add the entity into its chunk. This means it will have all
        //of its components, but they won't be assigned until iteration over the given command pool assigns them
        case EntityCommandType::CreateAndAddComponents: {
          const CreateCommandChunkID& createCommand = (*commandIt).get<const CreateCommandChunkID>();
          //See if the chunk already exists
          auto chunk = registry.findChunk(createCommand.mChunkID);
          //If the chunk doesn't exist it needs to be created
          if(chunk == registry.endChunks()) {
            chunk = createCommand.mCreateChunk(registry);
            assert(chunk != registry.endChunks() && "New chunk creation should always succeed");
          }

          //Add the entity to the existing or newly created chunk
          if(!chunk.tryAddDefaultConstructedEntity((*commandIt).entity(), generator)) {
            assert(false && "generated id should always result in new entities");
          }
          break;
        }
        case EntityCommandType::AddRemoveComponents:
          //This can happen if an entity command gets recycled from a create command to add/remove before clearing
          break;
      }
      //Mark as processed
      type = EntityCommandType::SkipCommand;
    }

    LinearEntity _createCommandEntity(EntityCommandType command) {
      const LinearEntity result = mEntityGenerator->getOrCreateId();
      _createCommandEntity(result, command);
      return result;
    }

    template<class ComponentT>
    ComponentT* _addComponentCommand(const CommandEntity& entity, ComponentCommandType type) {
      ComponentCommand<ComponentT> componentCmd{ type };
      return static_cast<ComponentT*>(_addComponentCommand(entity, type, _getStorageInfo<ComponentT, LinearEntity>(), RuntimeComponentTypes::create<ComponentT>(), &componentCmd));
    }

    static StorageInfo<CommandEntity> _toCommandStorage(const StorageInfo<LinearEntity>& storage, const typeId_t<CommandEntity>& type, const IRuntimeTraits& traits) {
      StorageInfo<CommandEntity> result;
      result.mComponentTraits = &traits;
      result.mCreateStorage = storage.mCreateStorage;
      result.mStorageData =  storage.mStorageData;
      result.mType = type;
      return result;
    }

    static StorageInfo<LinearEntity> _toDestinationStorage(const StorageInfo<CommandEntity>& storage, const typeId_t<LinearEntity>& type) {
      StorageInfo<LinearEntity> result;
      result.mComponentTraits = storage.mComponentTraits;
      result.mCreateStorage = storage.mCreateStorage;
      result.mStorageData =  storage.mStorageData;
      result.mType = type;
      return result;
    }

    void* _addComponentCommand(const CommandEntity& entity, ComponentCommandType type, const StorageInfo<LinearEntity>& info, const RuntimeComponentTypes& runtimeTypes, void* component) {
      //Add new or replace existing type
      const StorageInfo<CommandEntity> commandStorage = _toCommandStorage(info, runtimeTypes.mCommandType, *_getStorageInfo<RuntimeComponentCommand, CommandEntity>().mComponentTraits);
      mCommandEntities.addRuntimeComponent(entity, component, commandStorage);

      auto pool = mCommandEntities.findPool(runtimeTypes.mComponentType);
      const bool isExistingPool = pool != mCommandEntities.poolsEnd();
      void* result = nullptr;
      //Add the command based on type
      switch(type) {
        case ComponentCommandType::AddComponent:
        case ComponentCommandType::AssignComponent: {
          const StorageInfo<CommandEntity> componentStorage = _toCommandStorage(info, runtimeTypes.mComponentType, *info.mComponentTraits);
          result = mCommandEntities.addRuntimeComponent(entity, nullptr, componentStorage);
          break;
        }
        case ComponentCommandType::RemoveComponent:
          //Nothing to do here, processor knows what to do based on ComponentCommand alone
          break;
        case ComponentCommandType::SkipCommand:
          assert(false && "Shouldn't be called with skip");
          break;
      }

      if(isExistingPool) {
        return result;
      }
      //If this is a new pool, initialize the shared component on it
      const StorageInfo<CommandEntity> componentStorage = _toCommandStorage(info, runtimeTypes.mComponentType, *info.mComponentTraits);
      pool = mCommandEntities.getOrCreatePool(componentStorage);
      assert(pool != mCommandEntities.poolsEnd() && "Pool should exist after adding it");

      ComponentCommandProcessor processor;
      processor.processCommands = &_processAllRuntimeComponentCommands;
      processor.mTypes = runtimeTypes;
      processor.setComponentCommand = _setComponentCommand;
      pool.getSharedComponents().emplace<ComponentCommandProcessor>(processor);
      return result;
    }

    void _setComponentCommandOnAllComponents(const CommandEntity& entity, ComponentCommandType type) {
      for(auto pool = mCommandEntities.poolsBegin(); pool != mCommandEntities.poolsEnd(); ++pool) {
        if(auto processor = pool.getSharedComponents().emplace<ComponentCommandProcessor>(); processor.setComponentCommand) {
          processor.setComponentCommand(mCommandEntities, type, entity, processor.mTypes);
        }
      }
    }

    static void _setComponentCommand(CommandRegistry& commandEntities, ComponentCommandType type, const CommandEntity& entity, const RuntimeComponentTypes& types) {
      auto storage = _getStorageInfo<RuntimeComponentCommand, CommandEntity>();
      storage.mType = types.mCommandType;
      //This works because all template instantiations have the same layout
      if(void* command = commandEntities.tryGetComponent(entity, storage)) {
        static_cast<RuntimeComponentCommand*>(command)->mType = type;
      }
    }

    void _createCommandEntity(const LinearEntity& forEntity, EntityCommandType command) {
      if(!mCommandEntities.tryCreateEntity(forEntity.mData.mRawId)) {
        assert(false && "ID generator should always generate new ids");
      }
      mCommandEntities.addComponent<EntityCommandType>(forEntity.mData.mRawId, command);
    }

    void _destroyCommandEntity(const LinearEntity& entity, EntityCommandType& command) {
      //Actually destroying would disrupt command order due to swap remove, so instead, mark as skipped and remove all component requests
      command = EntityCommandType::SkipCommand;
      mCommandEntities.removeAllComponentsExcept<EntityCommandType>(entity.mData.mRawId);
    }

    //Use the sparse style registry because chunk approach isn't important here and command order is desired
    //which will be provided by packed component order
    EntityRegistry<CommandEntity> mCommandEntities;
    std::shared_ptr<IndependentEntityGenerator> mEntityGenerator;
  };
};