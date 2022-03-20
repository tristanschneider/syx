#include "ecs/system/EditorSystem.h"

#include "ecs/component/EditorComponents.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/component/GameobjectComponent.h"
#include "ecs/component/ImGuiContextComponent.h"
#include "ecs/component/SpaceComponents.h"
#include "ecs/component/UriActivationComponent.h"

#include "imgui/imgui.h"

const char* EditorSystem::WINDOW_NAME = "Objects";
const char* EditorSystem::NEW_OBJECT_LABEL = "New Object";
const char* EditorSystem::DELETE_OBJECT_LABEL = "Delete Object";
const char* EditorSystem::OBJECT_LIST_NAME = "ScrollView";

namespace EditorImpl {
  using namespace Engine;
  using ImGuiView = View<Write<ImGuiContextComponent>>;
  using SelectedView = View<Read<SelectedComponent>>;
  using ObjectsView = View<Read<GameobjectComponent>, OptionalRead<NameTagComponent>>;
  using BrowserModifier = EntityModifier<SelectedComponent>;

  void tickSceneBrowser(SystemContext<ImGuiView, SelectedView, ObjectsView, EntityFactory, BrowserModifier>& context) {
    if(!context.get<ImGuiView>().tryGetFirst()) {
      return;
    }
    ImGui::Begin(EditorSystem::WINDOW_NAME);
    auto& selected = context.get<SelectedView>();
    auto& objects = context.get<ObjectsView>();
    auto modifier = context.get<BrowserModifier>();
    auto factory = context.get<EntityFactory>();

    if(ImGui::Button(EditorSystem::NEW_OBJECT_LABEL)) {
      modifier.removeComponentsFromAllEntities<SelectedComponent>();
      auto&& [entity, a, b, nameTag ] = factory.createAndGetEntityWithComponents<GameobjectComponent, SelectedComponent, NameTagComponent>();
      nameTag.get().mName = "New Object";
    }

    if(ImGui::Button(EditorSystem::DELETE_OBJECT_LABEL)) {
      //Delete all selected entities, meaning any in these chunks
      for(auto chunk : selected.chunks()) {
        chunk.clear(factory);
      }
    }

    ImGui::BeginChild(EditorSystem::OBJECT_LIST_NAME, ImVec2(0, 0), true);

    std::string name;
    for(auto obj : objects) {
      const auto* nameTag = obj.tryGet<const NameTagComponent>();
      const Entity entity = obj.entity();
      const uint64_t entityID = obj.entity().mData.mRawId;
      //Make handle a hidden part of the id
      name = (nameTag ? nameTag->mName : std::to_string(entityID)) + "##" + std::to_string(entityID);

      if(ImGui::Selectable(name.c_str(), selected.find(obj.entity()) != selected.end())) {
        //If a new item is selected, clear the selection and select the new one
        modifier.removeComponentsFromAllEntities<SelectedComponent>();
        //TODO: I think this will invalidate the iterator
        modifier.addComponent<SelectedComponent>(entity);
      }
    }
    ImGui::EndChild();

    ImGui::End();
  }
}

std::shared_ptr<Engine::System> EditorSystem::sceneBrowser() {
  return ecx::makeSystem("SceneBrowser", &EditorImpl::tickSceneBrowser, 0);
}

std::shared_ptr<Engine::System> EditorSystem::init() {
  using namespace Engine;
  using Modifier = EntityModifier<EditorContextComponent,
    EditorSavedSceneComponent,
    EditorSceneReferenceComponent,
    SpaceTagComponent
  >;

  return ecx::makeSystem("EditorInit", [](SystemContext<EntityFactory, Modifier>& context) {
    auto factory = context.get<EntityFactory>();
    auto modifier = context.get<Modifier>();

    auto editorSpace = factory.createEntity();
    modifier.addComponent<SpaceTagComponent>(editorSpace);

    auto editorContext = factory.createEntity();
    modifier.addComponent<EditorContextComponent>(editorContext);
    modifier.addComponent<EditorSavedSceneComponent>(editorContext);
    modifier.addComponent<EditorSceneReferenceComponent>(editorContext, EditorSceneReferenceComponent{ editorSpace });
  });
}

std::shared_ptr<Engine::System> EditorSystem::createUriListener() {
  using namespace Engine;
  using SpaceView = View<Include<SpaceTagComponent>, OptionalRead<DefaultPlaySpaceComponent>>;
  using EditorContextView = View<Include<EditorContextComponent>, Write<EditorSavedSceneComponent>, Read<EditorSceneReferenceComponent>>;
  using UriView = View<Read<UriActivationComponent>>;
  using GlobalView = View<Write<FileSystemComponent>>;
  using Modifier = EntityModifier<ClearSpaceComponent, LoadSpaceComponent>;

  return ecx::makeSystem("EditorUriListener", [](SystemContext<SpaceView, EditorContextView, UriView, GlobalView, Modifier>& context) {
    const static std::string LOAD_SCENE = "loadScene";
    auto global = context.get<GlobalView>().tryGetFirst();
    auto editorContext = context.get<EditorContextView>().tryGetFirst();
    if(!global || !editorContext) {
      return;
    }
    FileSystemComponent& fs = global->get<FileSystemComponent>();
    EditorSavedSceneComponent& savedScene = editorContext->get<EditorSavedSceneComponent>();
    auto modifier = context.get<Modifier>();
    const Entity editorSpace = editorContext->get<const EditorSceneReferenceComponent>().mScene;

    for(auto&& chunk : context.get<UriView>().chunks()) {
      const std::vector<UriActivationComponent>& uris = *chunk.tryGet<const UriActivationComponent>();
      //Component is swapped to a new chunk if the new event types are appended to it
      for(size_t i = 0; i < uris.size();) {
        auto params = UriActivationComponent::parseUri(uris[i].mUri);
        if(auto it = params.find(LOAD_SCENE); it != params.end() && fs.get().fileExists(it->second.c_str())) {
          //Update working scene to this
          savedScene.mFilename = FilePath(it->second.c_str());
          auto messageEntity = chunk.indexToEntity(i);
          //Clear existing contents from editor space
          modifier.addDeducedComponent(messageEntity, ClearSpaceComponent{ editorSpace });
          //Load new scene into editor space
          modifier.addDeducedComponent(messageEntity, LoadSpaceComponent{ editorSpace, savedScene.mFilename });
        }
        else {
          //Entity was not moved between chunks so manually increment
          ++i;
        }
      }
    }
  });
}
