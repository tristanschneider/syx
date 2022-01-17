#include "ecs/system/EditorSystem.h"

#include "ecs/component/EditorComponents.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/component/SpaceComponents.h"
#include "ecs/component/UriActivationComponent.h"

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
