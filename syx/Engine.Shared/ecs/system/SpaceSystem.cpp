#include "Precompile.h"
#include "ecs/system/SpaceSystem.h"

#include <charconv>
#include "ecs/component/FileSystemComponent.h"
#include "ecs/component/SpaceComponents.h"
#include "ecs/system/RemoveFromAllEntitiesSystem.h"

//TODO: this is goofy, in trying to avoid forcing a file format I have invented my own bad file format
//All of the string specific logic should go either with lua serializer or some other approach
namespace {
  //Delimiter used to find different component sections
  //This is problematic if it's possible for this to appear in the middle of one of the other sections
  const std::string_view SECTION_DELIMITER = "_-=+_-=+--+_++_";
  const std::string_view ENTITIES_SECTION = "entities";

  void append(std::vector<uint8_t>& buffer, std::string_view str) {
      size_t index = buffer.size();
      buffer.resize(buffer.size() + str.size());
      std::memcpy(&buffer[index], str.data(), str.size());
  }

  //TODO: another possibility would be putting the responsibility of this on the serailization implementation, so using something like json/lua all the way through
  void writeSection(std::vector<uint8_t>& buffer, const std::string& sectionName, const ParsedSpaceContentsComponent::Section& section) {
    append(buffer, SECTION_DELIMITER);
    append(buffer, sectionName);
    append(buffer, SECTION_DELIMITER);
    append(buffer, std::string_view(reinterpret_cast<const char*>(section.mBuffer.data()), section.mBuffer.size()));
  }

  struct ParsedSection {
    std::string_view mName;
    std::string_view mContents;
    size_t mEndIndex = 0;
  };

  std::optional<ParsedSection> parseSection(const std::vector<uint8_t>& buffer, size_t index) {
    if(index >= buffer.size()) {
      return {};
    }
    std::string_view strBuffer(reinterpret_cast<const char*>(&buffer[index]), buffer.size() - index);
    if(const size_t sectionDelimiter = strBuffer.find(SECTION_DELIMITER, 0); sectionDelimiter != std::string_view::npos) {
      const size_t nameBegin = sectionDelimiter + SECTION_DELIMITER.size();
      if(const size_t nameDelimiter = strBuffer.find(SECTION_DELIMITER, nameBegin); nameDelimiter != std::string_view::npos) {
        const size_t nameEnd = nameDelimiter;
        const size_t sectionBegin = nameEnd + SECTION_DELIMITER.size();
        if(const size_t sectionEndDelimiter = strBuffer.find(SECTION_DELIMITER, sectionBegin); sectionEndDelimiter != std::string_view::npos) {
          ParsedSection result;
          result.mName = strBuffer.substr(nameBegin, nameEnd - nameBegin);
          result.mContents = strBuffer.substr(sectionBegin, sectionEndDelimiter - sectionBegin);
          //This starts at the ending delimiter (not after) because that doubles as the start of the next section
          result.mEndIndex = sectionEndDelimiter + index;
          return result;
        }
      }
    }
    return {};
  }

  std::optional<std::pair<std::string, ParsedSpaceContentsComponent::Section>> readNextSection(const std::vector<uint8_t>& buffer, size_t& index) {
    if(std::optional<ParsedSection> section = parseSection(buffer, index)) {
      std::vector<uint8_t> resultBuffer(section->mContents.size() + 1, uint8_t(0));
      std::memcpy(resultBuffer.data(), section->mContents.data(), section->mContents.size());
      //Null terminate just in case
      resultBuffer.back() = uint8_t(0);

      index = section->mEndIndex;
      return std::make_pair(std::string(section->mName), ParsedSpaceContentsComponent::Section{ std::move(resultBuffer) });
    }
    return {};
  }

  void writeHeader(std::vector<uint8_t>& buffer, const std::vector<Engine::Entity>& entities) {
    append(buffer, SECTION_DELIMITER);
    append(buffer, ENTITIES_SECTION);
    append(buffer, SECTION_DELIMITER);
    if(!entities.empty()) {
      //Write comma separated list of entity ids
      for(size_t i = 0; i < entities.size() - 1; ++i) {
        append(buffer, std::to_string(entities[i].mData.mParts.mEntityId));
        buffer.push_back(uint8_t(','));
      }
      append(buffer, std::to_string(entities.back().mData.mParts.mEntityId));
    }
  }

  std::vector<Engine::Entity> readHeader(const std::vector<uint8_t>& buffer, size_t& index) {
    //Don't keep searching on name mismatch, file must begin with this section
    if(auto section = parseSection(buffer, index); section && section->mName == ENTITIES_SECTION) {
      std::vector<Engine::Entity> result;
      while(!section->mContents.empty()) {
        size_t idEnd = section->mContents.find_first_of(',');
        uint32_t entity = 0;
        const char* idBeginStr = &section->mContents.front();
        const char* idEndStr = idEnd == std::string_view::npos ? ((&section->mContents.back()) + 1) : &section->mContents[idEnd];
        if(auto convResult = std::from_chars(idBeginStr, idEndStr, entity); convResult.ec == std::errc()) {
          result.push_back(Engine::Entity(entity, uint32_t(0)));
        }

        section->mContents = idEnd == std::string_view::npos ? std::string_view() : section->mContents.substr(idEnd + 1);
      }

      index = section->mEndIndex;
      return result;
    }
    return {};
  }
}

std::shared_ptr<Engine::System> SpaceSystem::clearSpaceSystem() {
  using namespace Engine;
  using SpaceView = View<Include<SpaceTagComponent>>;
  using EntityView = View<Read<InSpaceComponent>>;
  using MessageView = View<Read<ClearSpaceComponent>>;
  return ecx::makeSystem("ClearSpaceSystem", [](SystemContext<EntityFactory, SpaceView, EntityView, MessageView>& context) {
    EntityFactory factory = context.get<EntityFactory>();
    auto& spaces = context.get<SpaceView>();

    //Gather all spaces to clear from all ClearSpaceComponents
    std::vector<Entity> spacesToClear;
    for(auto messageChunks : context.get<MessageView>().chunks()) {
      for(const ClearSpaceComponent& msg : *messageChunks.tryGet<const ClearSpaceComponent>()) {
        if(auto spaceToClear = spaces.find(msg.mSpace); spaceToClear != spaces.end()) {
          spacesToClear.push_back((*spaceToClear).entity());
        }
      }
    }

    //Remove all entities that are in cleared spaces
    if(!spacesToClear.empty()) {
      //TODO: optimization with compile time space tags
      for(auto entityChunks : context.get<EntityView>().chunks()) {
        const auto entities = entityChunks.tryGet<const InSpaceComponent>();
        for(size_t i = 0; i < entities->size();) {
          if(std::find(spacesToClear.begin(), spacesToClear.end(), entities->at(i).mSpace) != spacesToClear.end()) {
            //TODO: this operation is more expensive than it needs to be, should use the chunk to avoid lookups
            factory.destroyEntity(entityChunks.indexToEntity(i));
          }
          else {
            ++i;
          }
        }
      }
    }
  });
}

std::shared_ptr<Engine::System> SpaceSystem::beginLoadSpaceSystem() {
  using namespace Engine;
  using MessageView = View<Read<LoadSpaceComponent>>;
  using AlreadyLoadingView = View<Include<SpaceLoadingComponent>>;
  using Modifier = EntityModifier<SpaceLoadingComponent, FileReadRequest>;
  return ecx::makeSystem("beginLoadSpace", [](SystemContext<MessageView, Modifier, AlreadyLoadingView>& context) {
    auto modifier = context.get<Modifier>();
    auto& alreadyLoading = context.get<AlreadyLoadingView>();
    for(auto messageChunk : context.get<MessageView>().chunks()) {
      for(const auto& message : *messageChunk.tryGet<const LoadSpaceComponent>()) {
        if(alreadyLoading.find(message.mSpace) == alreadyLoading.end()) {
          modifier.addDeducedComponent(message.mSpace, FileReadRequest{ message.mToLoad });
          modifier.addComponent<SpaceLoadingComponent>(message.mSpace);
        }
      }
    }
  });
}

std::shared_ptr<Engine::System> SpaceSystem::parseSceneSystem() {
  using namespace Engine;
  //TODO: what about failure?
  using SpaceView = View<Read<FileReadSuccessResponse>, Read<FileReadRequest>, Include<SpaceLoadingComponent>, Exclude<ParsedSpaceContentsComponent>>;
  using Modifier = EntityModifier<ParsedSpaceContentsComponent>;
  return ecx::makeSystem("parseScene", [](SystemContext<SpaceView, Modifier>& context) {
    auto modifier = context.get<Modifier>();
    auto& view = context.get<SpaceView>();
    for(auto chunk : view.chunks()) {
      const auto& spaces = *chunk.tryGet<const FileReadSuccessResponse>();

      for(size_t i = 0; i < spaces.size();) {
        size_t index = 0;
        const std::vector<uint8_t>& buffer = spaces[i].mBuffer;
        ParsedSpaceContentsComponent newContents;
        newContents.mFile = chunk.tryGet<const FileReadRequest>()->front().mToRead;
        newContents.mNewEntities = readHeader(buffer, index);
        //TODO: what to do on failure case?
        if(!newContents.mNewEntities.empty()) {
          while(auto section = readNextSection(buffer, index)) {
            newContents.mSections.emplace(std::move(*section));
          }
        }
        modifier.addComponent<ParsedSpaceContentsComponent>(chunk.indexToEntity(i), std::move(newContents));
      }
    }
  });
}

std::shared_ptr<Engine::System> SpaceSystem::createSpaceEntitiesSystem() {
  using namespace Engine;
  using Modifier = EntityModifier<InSpaceComponent, SpaceFillingEntitiesComponent>;
  using SpaceView = View<Write<ParsedSpaceContentsComponent>, Exclude<SpaceFillingEntitiesComponent>>;
  return ecx::makeSystem("createSpaceEntities", [](SystemContext<EntityFactory, Modifier, SpaceView>& context) {
    auto modifier = context.get<Modifier>();
    auto factory = context.get<EntityFactory>();

    for(auto spaceChunk : context.get<SpaceView>().chunks()) {
      //Create all entities
      auto& parsedSpaceContent = *spaceChunk.tryGet<ParsedSpaceContentsComponent>();
      for(size_t i = 0; i < parsedSpaceContent.size(); ++i) {
        for(const Entity& entity : parsedSpaceContent[i].mNewEntities) {
          //TODO: what if this fails? Remap entity id?
          if(std::optional<Entity> created = factory.tryCreateEntityWithComponents(entity)) {
            modifier.addComponent<InSpaceComponent>(*created, spaceChunk.indexToEntity(i));
          }
        }
      }

      //Tag the space as loading entities
      while(spaceChunk.size()) {
        modifier.addComponent<SpaceFillingEntitiesComponent>(spaceChunk.indexToEntity(0));
      }
    }
  });
}

std::shared_ptr<Engine::System> SpaceSystem::completeSpaceLoadSystem() {
  using namespace Engine;
  //Remove intermediate loading components from space at the end of the tick that it had SpaceFillingEntitiesComponent
  //In the future there may need to be a mechanism to delay the destruction for multiple frames to allow deferred loading
  //TODO read failure response
  return removeFomAllEntitiesInView<View<Include<SpaceLoadingComponent>, Include<SpaceFillingEntitiesComponent>, Include<FileReadSuccessResponse>>
    , SpaceLoadingComponent
    , ParsedSpaceContentsComponent
    , SpaceFillingEntitiesComponent
    , FileReadRequest
    , FileReadSuccessResponse
  >();
}

std::shared_ptr<Engine::System> SpaceSystem::beginSaveSpaceSystem() {
  using namespace Engine;
  using RequestView = View<Read<SaveSpaceComponent>>;
  using SpaceView = View<Include<SpaceTagComponent>, Exclude<FileWriteRequest>>;
  using Modifier = EntityModifier<SpaceSavingComponent, SpaceFillingEntitiesComponent, ParsedSpaceContentsComponent>;
  return ecx::makeSystem("beginSaveSpace", [](SystemContext<RequestView, Modifier, SpaceView>& context) {
    auto modifier = context.get<Modifier>();
    auto& spaceView = context.get<SpaceView>();
    for(auto chunks : context.get<RequestView>().chunks()) {
      for(const SaveSpaceComponent& saveRequest : *chunks.tryGet<const SaveSpaceComponent>()) {
        if(const Entity space = saveRequest.mSpace; spaceView.find(space) != spaceView.end()) {
          //TODO: should this check if it's a valid path before adding the components
          modifier.addComponent<ParsedSpaceContentsComponent>(space).mFile = saveRequest.mToSave;
          modifier.addComponent<SpaceFillingEntitiesComponent>(space);
          modifier.addComponent<SpaceSavingComponent>(space);
        }
      }
    }
  });
}

std::shared_ptr<Engine::System> SpaceSystem::createSerializedEntitiesSystem() {
  using namespace Engine;
  using SpaceView = View<Include<SpaceFillingEntitiesComponent>, Write<ParsedSpaceContentsComponent>, Exclude<FileWriteRequest>>;
  using EntityView = View<Read<InSpaceComponent>>;
  return ecx::makeSystem("createSerializedEntities", [](SystemContext<SpaceView, EntityView>& context) {
    for(auto request : context.get<SpaceView>()) {
      const Entity space = request.entity();
      auto& spaceContents = request.get<ParsedSpaceContentsComponent>();

      //Find all entities in this space and add them to the list
      for(auto entity : context.get<EntityView>()) {
        if(entity.get<const InSpaceComponent>().mSpace == space) {
          spaceContents.mNewEntities.push_back(entity.entity());
        }
      }
    }
  });
}

std::shared_ptr<Engine::System> SpaceSystem::serializeSpaceSystem() {
  using namespace Engine;
  using SpaceView = View<Read<ParsedSpaceContentsComponent>, Exclude<FileWriteRequest>>;
  using Modifier = EntityModifier<FileWriteRequest>;
  return ecx::makeSystem("serializeSpace", [](SystemContext<SpaceView, Modifier, EntityFactory>& context) {
    for(auto space : context.get<SpaceView>()) {
      //Coalesce all sections into a single buffer to submit the file write request
      const auto& contents = space.get<const ParsedSpaceContentsComponent>();
      std::vector<uint8_t> buffer;

      writeHeader(buffer, contents.mNewEntities);

      for(const auto& pair : contents.mSections) {
        writeSection(buffer, pair.first, pair.second);
      }

      //Write final delimiter after all sections
      append(buffer, SECTION_DELIMITER);

      //Submit the file write request
      Entity request = space.entity();
      auto modifier = context.get<Modifier>();
      modifier.addComponent<FileWriteRequest>(request, contents.mFile, std::move(buffer));
    }
  });
}

std::shared_ptr<Engine::System> SpaceSystem::completeSpaceSaveSystem() {
  using namespace Engine;
  //TODO file write failure
  return removeFomAllEntitiesInView<View<Include<SpaceSavingComponent>, Include<SpaceFillingEntitiesComponent>, Include<FileWriteSuccessResponse>>
    , SpaceSavingComponent
    , ParsedSpaceContentsComponent
    , SpaceFillingEntitiesComponent
    , FileWriteRequest
    , FileWriteSuccessResponse
  >();
}
