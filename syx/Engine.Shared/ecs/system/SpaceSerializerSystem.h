#pragma once

#include "ecs/ECS.h"
#include "ecs/component/SpaceComponents.h"

//Implement a template like this for ComponentSerializeSystem
template<class ComponentT>
struct ComponentSerialize {
  using Components = std::vector<std::pair<Engine::Entity, ComponentT>>;
  using Buffer = std::vector<uint8_t>;

  //Not implemented, shown here to demonstrate what the template should look like
  static Buffer serialize(const Components&);
  static Components deserialize(const Buffer&);
};

template<class ComponentT, class SerializerT>
struct ComponentSerializeSystem {
  static std::shared_ptr<Engine::System> createSerializer() {
    using namespace Engine;
    using SpaceView = View<Include<SpaceSavingComponent>, Include<SpaceFillingEntitiesComponent>, Write<ParsedSpaceContentsComponent>>;
    using EntityView = View<Read<ComponentT>>;

    return ecx::makeSystem("SerializeComponent", [](SystemContext<SpaceView, EntityView>& context) {
      EntityView& entities = context.get<EntityView>();
      //Iterate over all spaces that need to be serialized
      for(auto spaceChunk : context.get<SpaceView>().chunks()) {
        for(auto& spaceContents : *spaceChunk.tryGet<ParsedSpaceContentsComponent>()) {
          //Build the list of components to pass to the serializer
          typename ComponentSerialize<ComponentT>::Components components;
          components.reserve(spaceContents.mNewEntities.size());
          //See if the entity has the desired component
          //TODO: this is a high price to pay for uncommon components, might make more sense to iterate over entities with the component
          for(const Entity& entity : spaceContents.mNewEntities) {
            if(auto found = entities.find(entity); found != entities.end()) {
              components.push_back(std::make_pair(entity, (*found).get<const ComponentT>()));
            }
          }

          //Save the serialized section in the ParsedSpaceContentsComponent
          //TODO, split this out from the serialization step so that serializers don't need write access to the component for as long
          spaceContents.mSections[ecx::StaticTypeInfo<ComponentT>::getTypeName()] = ParsedSpaceContentsComponent::Section{ SerializerT::serialize(components) };
        }
      }
    });
  }

  static std::shared_ptr<Engine::System> createDeserializer() {
    using namespace Engine;
    using SpaceView = View<Include<SpaceLoadingComponent>, Read<ParsedSpaceContentsComponent>, Include<SpaceFillingEntitiesComponent>>;
    using Modifier = EntityModifier<ComponentT>;

    return ecx::makeSystem("DeserializeComponent", [](SystemContext<SpaceView, Modifier>& context) {
      Modifier modifier = context.get<Modifier>();
      //Iterate over all spaces that need deserializing
      for(auto spaceChunk : context.get<SpaceView>().chunks()) {
        for(auto& spaceContents : *spaceChunk.tryGet<const ParsedSpaceContentsComponent>()) {
          //See if the section for this component exists
          if(auto foundSection = spaceContents.mSections.find(ecx::StaticTypeInfo<ComponentT>::getTypeName()); foundSection != spaceContents.mSections.end()) {
            //Deserialize section into a list of components
            //TODO: split this to its own step to avoid doing the work while holding the Modifier
            typename ComponentSerialize<ComponentT>::Components components = SerializerT::deserialize(foundSection->second.mBuffer);
            //Add deserialized component list to the real entities
            //TODO: registry support to add these all at once
            for(auto& pair : components) {
              if(auto remapping = spaceContents.mRemappings.find(pair.first); remapping != spaceContents.mRemappings.end()) {
                pair.first = remapping->second;
              }
              modifier.addComponent<ComponentT>(pair.first, pair.second);
            }
          }
        }
      }
    });
  }
};