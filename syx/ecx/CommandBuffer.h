#pragma once

#include "LinearEntityRegistry.h"

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

    CommandBuffer(IDGenerator idGen)
      : mIDGen(std::move(idGen)) {
    }

    template<class... Components>
    auto createAndGetEntityWithComponents() {
      LinearEntity entity = _createCommandEntity(EntityCommandType::CreateAndAddComponents);
      CreateCommandChunkID chunk;
      chunk.mChunkID = LinearEntity::buildChunkId<Components...>();
      chunk.mCreateChunk = [](DestinationRegistry& registry) {
        return registry.getOrCreateChunk<Components...>();
      };
      mCommandEntities.addComponent<CreateCommandChunkID>(entity, std::move(chunk));
      mCommandEntities.addComponent<EntityAddRemoveFilterTag>(entity);
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
            _setComponentCommandOnAllComponents(entity, ComponentCommandType::SkipCommand);
            *existingCommand = EntityCommandType::SkipCommand;
            return;
          case EntityCommandType::AddRemoveComponents:
            //If this command was for adding some components, erase the add/remove commands and replace with a remove entity command
            _setComponentCommandOnAllComponents(entity, ComponentCommandType::SkipCommand);
            *existingCommand = EntityCommandType::RemoveEntity;
            return;
        }
      }
      else {
        //A command does not already exist for this, create it
        _createCommandEntity(entity, EntityCommandType::RemoveEntity);
      }
      mCommandEntities.addComponent<EntityAddRemoveFilterTag>(entity);
    }

    template<class Component>
    Component& addComponent(const LinearEntity& entity) {
      return *_addOrRemoveCommandComponent<Component>(entity, ComponentCommandType::AddComponent);
    }

    template<class Component>
    void removeComponent(const LinearEntity& entity) {
      _addOrRemoveCommandComponent<Component>(entity, ComponentCommandType::RemoveComponent);
    }

    void processAllCommands(EntityRegistry<LinearEntity>& registry) {
      EntityCommandView entityCommands(mCommandEntities);
      //First ensure all necessary entities have been created or removed
      for(auto it = entityCommands.begin(); it != entityCommands.end(); ++it) {
        EntityCommandType& type = (*it).get<EntityCommandType>();
        _processEntityCommand(registry, type, it);
      }

      //Process per-component commands of each pool
      for(auto pool = mCommandEntities.poolsBegin(); pool != mCommandEntities.poolsEnd(); ++pool) {
        //Processor will be empty for pools unrelated to the commands
        if(const ComponentCommandProcessor& processor = pool.getSharedComponents().emplace<ComponentCommandProcessor>(); processor.processCommands) {
          processor.processCommands(mCommandEntities, registry);
        }
      }
    }

    //Process all commands that this component type are involved in
    //This can include creation commands which affect other component types
    template<class Component>
    void processCommandsForComponent(DestinationRegistry& registry) {
      _processAllEntityCommands(mCommandEntities, registry);
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
      void (*processCommands)(CommandRegistry&, DestinationRegistry&) = nullptr;
      void (*setComponentCommand)(CommandRegistry&, ComponentCommandType, const CommandEntity&);
    };

    template<class Component>
    void _updateComponentCommandForAddRemove(const CommandEntity& entity, EntityCommandType existingCommand, ComponentCommandType& type) {
      ComponentCommand<Component>* existingComponentCommand = mCommandEntities.tryGetComponent<ComponentCommand<Component>>(entity);
      switch(existingCommand) {
      //If this is already an add/remove case then nothing special needed
      case EntityCommandType::AddRemoveComponents:
        break;
      //If this is a new entity, the chunk id needs to be updated then the value assigned
      case EntityCommandType::CreateAndAddComponents: {
        //Chunk needs to be updated if this is not a redundant command
        const bool updateChunkID = !existingComponentCommand || existingComponentCommand->mType != type;
        if(updateChunkID) {
          CreateCommandChunkID& chunk = mCommandEntities.getComponent<CreateCommandChunkID>(entity);
          chunk.mChunkID = LinearEntity::buildChunkId<Component>(chunk.mChunkID);
          auto prev = chunk.mCreateChunk;
          chunk.mCreateChunk = [prev, type](DestinationRegistry& registry) {
            switch(type) {
              case ComponentCommandType::AssignComponent:
              case ComponentCommandType::SkipCommand:
                assert(false && "unexpected type");
                break;
                //Fall through to return
              case ComponentCommandType::AddComponent:
                return registry.getOrCreateChunkAddedComponent<Component>(prev(registry));
              case ComponentCommandType::RemoveComponent:
                return registry.getOrCreateChunkRemovedComponent<Component>(prev(registry));
            }
            return registry.endChunks();
          };
        }
        //Since chunk was updated above, component will be added upon creation, and needs to be assigned
        type = ComponentCommandType::AssignComponent;
        break;
      }
      case EntityCommandType::RemoveEntity:
        if(type == ComponentCommandType::RemoveComponent) {
          //Calling remove component on a removed entity is fine I guess, otherwise fall through to assert
          break;
        }
      case EntityCommandType::SkipCommand:
        assert(false && "Invalid state");
        //TODO: what to do about the skip case?
      }
    }

    template<class Component>
    Component* _addOrRemoveCommandComponent(const LinearEntity& entity, ComponentCommandType type) {
      if(EntityCommandType* existingCommand = mCommandEntities.tryGetComponent<EntityCommandType>(entity.mData.mRawId)) {
        _updateComponentCommandForAddRemove<Component>(entity.mData.mRawId, *existingCommand, type);
      }
      else {
        _createCommandEntity(entity, EntityCommandType::AddRemoveComponents);
      }
      return _addComponentCommand<Component>(entity.mData.mRawId, type);
    }

    template<class Component>
    static void _processAllComponentCommands(CommandRegistry& commandEntities, DestinationRegistry& registry) {
      for(auto command : CommandView<Read<EntityCommandType>,
        Read<ComponentCommand<Component>>,
        OptionalWrite<Component>>(commandEntities)) {
        const LinearEntity entity(command.entity());

        //If iteration is always over an entire component pool this value would match on all entities so it could be a shared component
        const ComponentCommandType& componentCommandType = command.get<const ComponentCommand<Component>>().mType;
        switch(componentCommandType) {
          case ComponentCommandType::SkipCommand:
            continue;

          //Assign is from the creation of new entities default constructed in a given chunk
          //If the component doesn't exist it means the chunk creation didn't work properly
          case ComponentCommandType::AssignComponent:
            if(Component* storage = registry.tryGetComponent<Component>(entity)) {
              if(Component* toAssign = command.tryGet<Component>()) {
                *storage = std::move(*toAssign);
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
            if(Component* toAdd = command.tryGet<Component>()) {
              registry.addComponent<Component>(entity, std::move(*toAdd));
            }
            else {
              assert(false && "Command component storage should exist if ComponentCommand was added for its type");
            }
            break;

          case ComponentCommandType::RemoveComponent:
            registry.removeComponent<Component>(entity);
            break;
          }
        }
      }
      //Remove all commands of this type now that they've been processed
      //Don't delete the entire entity yet as there may still be other components involved
      commandEntities.findPool<ComponentCommand<Component>>().clear();
    }

    static void _processAllEntityCommands(CommandRegistry& commandEntities, DestinationRegistry& registry) {
      EntityCommandView entityCommands(commandEntities);
      //First ensure all necessary entities have been created or removed
      for(auto it = entityCommands.begin(); it != entityCommands.end(); ++it) {
        EntityCommandType& type = (*it).get<EntityCommandType>();
        _processEntityCommand(registry, type, it);
      }
    }

    static void _processEntityCommand(DestinationRegistry& registry, EntityCommandType& type, EntityCommandView::It& commandIt) {
      switch(type) {
        //If add/remove has already been processed, move on
        case EntityCommandType::SkipCommand:
          break;
        //This is an unprocessed remove entity command. Remove the entity then mark this command as processed
        case EntityCommandType::RemoveEntity:
          registry.destroyEntity((*commandIt).entity());
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
          if(!chunk.tryAddDefaultConstructedEntity((*commandIt).entity())) {
            assert(false && "generated id should always result in new entities");
          }
          break;
        }
        case EntityCommandType::AddRemoveComponents:
          assert(false && "Command shouldn't have EntityAddRemoveFilterTag if it's a command to add and remove components");
          break;
      }
      //Mark as processed
      type = EntityCommandType::SkipCommand;
    }

    LinearEntity _createCommandEntity(EntityCommandType command) {
      const LinearEntity result = mIDGen();
      _createCommandEntity(result, command);
      return result;
    }

    template<class ComponentT>
    ComponentT* _addComponentCommand(const CommandEntity& entity, ComponentCommandType type) {
      //Add new or replace existing type
      mCommandEntities.addComponent<ComponentCommand<ComponentT>>(entity, ComponentCommand<ComponentT>{ type });
      auto pool = mCommandEntities.findPool<ComponentT>();
      ComponentT* result = nullptr;
      //Add the command based on type
      switch(type) {
        case ComponentCommandType::AddComponent:
        case ComponentCommandType::AssignComponent:
          result = &mCommandEntities.addComponent<ComponentT>(entity);
          break;
        case ComponentCommandType::RemoveComponent:
          //Nothing to do here, processor knows what to do based on ComponentCommand alone
          break;
        case ComponentCommandType::SkipCommand:
          assert(false && "Shouldn't be called with skip");
          break;
      }

      if(pool != mCommandEntities.poolsEnd()) {
        return result;
      }
      //If this is a new pool, initialize the shared component on it
      pool = mCommandEntities.getOrCreatePool<ComponentT>();
      assert(pool != mCommandEntities.poolsEnd() && "Pool should exist after adding it");

      ComponentCommandProcessor processor;
      processor.processCommands = &_processAllComponentCommands<ComponentT>;
      processor.setComponentCommand = &_setComponentCommand<ComponentT>;
      pool.getSharedComponents().emplace<ComponentCommandProcessor>(processor);
      return result;
    }

    void _setComponentCommandOnAllComponents(const CommandEntity& entity, ComponentCommandType type) {
      for(auto pool = mCommandEntities.poolsBegin(); pool != mCommandEntities.poolsEnd(); ++pool) {
        if(auto processor = pool.getSharedComponents().emplace<ComponentCommandProcessor>(); processor.setComponentCommand) {
          processor.setComponentCommand(mCommandEntities, type, entity);
        }
      }
    }

    template<class ComponentT>
    static void _setComponentCommand(CommandRegistry& commandEntities, ComponentCommandType type, const CommandEntity& entity) {
      if(ComponentCommand<ComponentT>* command = commandEntities.tryGetComponent<ComponentCommand<ComponentT>>(entity)) {
        command->mType = type;
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
    IDGenerator mIDGen;
  };
};