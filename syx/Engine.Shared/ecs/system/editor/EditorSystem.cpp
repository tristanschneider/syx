#include "ecs/system/editor/EditorSystem.h"

#include "ecs/component/AssetComponent.h"
#include "ecs/component/EditorComponents.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/component/GameobjectComponent.h"
#include "ecs/component/GlobalCommandBufferComponent.h"
#include "ecs/component/ImGuiContextComponent.h"
#include "ecs/component/MessageComponent.h"
#include "ecs/component/PlatformMessageComponents.h"
#include "ecs/component/RawInputComponent.h"
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
  using EditorSceneRefView = View<Read<EditorSceneReferenceComponent>>;

  void tickSceneBrowser(SystemContext<ImGuiView, SelectedView, ObjectsView, EntityFactory, BrowserModifier, EditorSceneRefView>& context) {
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
      auto&& [entity, a, b, nameTag, inSpace] = factory.createAndGetEntityWithComponents<GameobjectComponent, SelectedComponent, NameTagComponent, InSpaceComponent>();
      nameTag.get().mName = "New Object";
      if(auto ref = context.get<EditorSceneRefView>().tryGetFirst()) {
        inSpace.get().mSpace = ref->get<const EditorSceneReferenceComponent>().mEditorScene;
      }
    }

    if(ImGui::Button(EditorSystem::DELETE_OBJECT_LABEL)) {
      //Delete all selected entities, meaning any in these chunks
      for(auto chunk : selected.chunks()) {
        chunk.clear(factory);
      }
    }

    ImGui::BeginChild(EditorSystem::OBJECT_LIST_NAME, ImVec2(0, 0), true);

    std::string name;
    //Sort manually otherwise UI order will change as entity moves between chunks
    std::vector<Entity> sortedObjects;
    for(auto obj : objects) {
      sortedObjects.push_back(obj.entity());
    }
    std::sort(sortedObjects.begin(), sortedObjects.end(), [](const Entity& l, const Entity& r) {
      return l.mData.mParts.mEntityId < r.mData.mParts.mEntityId;
    });

    for(auto sortedObj : sortedObjects) {
      auto found = objects.find(sortedObj);
      if(found == objects.end()) {
        continue;
      }
      auto obj = *found;

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

  using DropView = View<Read<OnFilesDroppedMessageComponent>>;
  void tickPlatformListener(SystemContext<DropView, EntityFactory>& context) {
    auto factory = context.get<EntityFactory>();
    for(auto&& drop : context.get<DropView>()) {
      for(const FilePath& file : drop.get<const OnFilesDroppedMessageComponent>().mFiles) {
        auto&& [ entity, asset ] = factory.createAndGetEntityWithComponents<AssetLoadRequestComponent>();
        asset.get().mPath = file;
      }
    }
  }

  using PlayStateView = View<Write<EditorPlayStateComponent>, Write<EditorPlayStateActionQueueComponent>>;
  using InputView = View<Read<RawInputComponent>>;
  using ToolboxSystemContext = SystemContext<ImGuiView, PlayStateView, InputView>;
  void _open(ToolboxSystemContext&) {
  }

  void _save(ToolboxSystemContext&) {
  }

  void _saveAs(ToolboxSystemContext&) {
  }

  void _play(EditorPlayState& state) {
    state = EditorPlayState::Playing;
  }

  void _pause(EditorPlayState& state) {
    state = EditorPlayState::Paused;
  }

  void _stop(EditorPlayState& state) {
    state = EditorPlayState::Stopped;
  }

  void _step(EditorPlayState& state) {
    state = EditorPlayState::Stepping;
  }

  void tickToolboxInput(const RawInputComponent& input, EditorPlayStateComponent& playState) {
    const bool ctrl = input.getKeyDownOrTriggered(Key::LeftCtrl) || input.getKeyDownOrTriggered(Key::RightCtrl);
    const bool shift = input.getKeyDownOrTriggered(Key::Shift);
    if(playState.mCurrentState == EditorPlayState::Playing) {
      if(shift && input.getKeyTriggered(Key::F5))
        _stop(playState.mCurrentState);
      else if(input.getKeyTriggered(Key::F6)) {
        _pause(playState.mCurrentState);
      }
    }
    else {
      if(ctrl && shift && input.getKeyTriggered(Key::KeyS)) {
        //_saveAs();
      }
      else if(ctrl && input.getKeyTriggered(Key::KeyS)) {
        //_save();
      }
      else if(ctrl && input.getKeyTriggered(Key::KeyO)) {
        //_open();
      }
      else if(input.getKeyTriggered(Key::F5)) {
        _play(playState.mCurrentState);
      }
      else if(input.getKeyTriggered(Key::F6) && playState.mCurrentState == EditorPlayState::Paused) {
        _step(playState.mCurrentState);
      }
    }
  }

  void tickToolbox(ToolboxSystemContext& context) {
    auto playState = context.get<PlayStateView>().tryGetFirst();
    auto imgui = context.get<ImGuiView>().tryGetFirst();
    auto input = context.get<InputView>().tryGetFirst();
    if(!playState || !input) {
      return;
    }
    EditorPlayStateComponent& state = playState->get<EditorPlayStateComponent>();
    EditorPlayState& currentState = state.mCurrentState;
    const RawInputComponent& rawInput = input->get<const RawInputComponent>();

    tickToolboxInput(rawInput, state);

    if(!imgui) {
      return;
    }

    ImGui::Begin("Toolbox", nullptr, ImGuiWindowFlags_MenuBar);
    if(ImGui::BeginMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("Open", "Ctrl+O")) {
              _open(context);
            }
            if(ImGui::MenuItem("Save", "Ctrl+S")) {
              _save(context);
            }
            if(ImGui::MenuItem("Save As", "Ctrl+Shift+S")) {
              _saveAs(context);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if(currentState == EditorPlayState::Stopped || currentState == EditorPlayState::Paused) {
      if(ImGui::Button("Play")) {
        _play(currentState);
      }
    }
    //TODO: some of these with the playing state don't make sense since all imgui is disabled during play state
    if(currentState == EditorPlayState::Stepping || currentState == EditorPlayState::Playing) {
      if(ImGui::Button("Pause")) {
        _pause(currentState);
      }
    }
    if(currentState == EditorPlayState::Paused) {
      ImGui::SameLine();
      if(ImGui::Button("Step")) {
        _step(currentState);
      }
    }
    if(currentState == EditorPlayState::Playing || currentState == EditorPlayState::Stepping || currentState == EditorPlayState::Paused) {
      ImGui::SameLine();
      if(ImGui::Button("Stop")) {
        _stop(currentState);
      }
    }

    ImGui::End();
  }

  using SpaceView = View<Include<SpaceTagComponent>, Write<TimescaleComponent>>;
  using SpaceCompleteView = View<Include<SpaceTagComponent>, Exclude<SpaceLoadingComponent>, Exclude<SpaceSavingComponent>>;
  using SceneRefView = View<Read<EditorSceneReferenceComponent>, Read<EditorSavedSceneComponent>>;
  using MessageView = View<Include<MessageComponent>>;
  using GlobalCommandView = View<Write<GlobalCommandBufferComponent>>;
  using PlayStateContext = SystemContext<PlayStateView, GlobalCommandView, SpaceView, SceneRefView, SpaceCompleteView, MessageView>;

  bool _isMessageProcessed(PlayStateContext& context, const Entity& message) {
    auto& view = context.get<MessageView>();
    return view.find(message) == view.end();
  }

  void _clearSpace(PlayStateContext& context, const Entity& space, EditorPlayStateActionQueueComponent& queue) {
    switch(queue.mState) {
    case EditorPlayStateActionQueueComponent::ActionState::Init: {
      if(auto cmd = context.get<GlobalCommandView>().tryGetFirst()) {
        auto c = cmd->get<GlobalCommandBufferComponent>().get<ClearSpaceComponent, MessageComponent>();
        auto&& [e, clearSpace, m] = c.createAndGetEntityWithComponents<ClearSpaceComponent, MessageComponent>();
        clearSpace->mSpace = space;
        queue.mPendingMessage = e;
      }
      break;
    }
    case EditorPlayStateActionQueueComponent::ActionState::Update:
      //Clear is complete once the message has been consumed
      if(_isMessageProcessed(context, queue.mPendingMessage)) {
        queue.mActions.pop();
      }
      break;
    }
  }

  void _clearPlaySpaceAction(void* data, EditorPlayStateActionQueueComponent& queue) {
    auto* context = reinterpret_cast<PlayStateContext*>(data);
    if(auto editor = context->get<SceneRefView>().tryGetFirst()) {
      _clearSpace(*context, editor->get<const EditorSceneReferenceComponent>().mPlayScene, queue);
    }
  }

  void _clearEditorSpaceAction(void* data, EditorPlayStateActionQueueComponent& queue) {
    auto* context = reinterpret_cast<PlayStateContext*>(data);
    if(auto editor = context->get<SceneRefView>().tryGetFirst()) {
      _clearSpace(*context, editor->get<const EditorSceneReferenceComponent>().mEditorScene, queue);
    }
  }

  //Await completion of a space save/load operation
  void _awaitSpaceCompletion(PlayStateContext& context, const Entity& space, EditorPlayStateActionQueueComponent& queue) {
    //View excludes pending coponents, so once it's found in this view the operation is complete and the action can be popped
    auto& view = context.get<SpaceCompleteView>();
    if(_isMessageProcessed(context, queue.mPendingMessage) && view.find(space) != view.end()) {
      queue.mActions.pop();
    }
  }

  void _saveEditorSpaceAction(void* data, EditorPlayStateActionQueueComponent& queue) {
    auto* context = reinterpret_cast<PlayStateContext*>(data);
    if(auto editor = context->get<SceneRefView>().tryGetFirst()) {
      switch(queue.mState) {
        case EditorPlayStateActionQueueComponent::ActionState::Init: {
          if(auto cmd = context->get<GlobalCommandView>().tryGetFirst()) {
            auto c = cmd->get<GlobalCommandBufferComponent>().get<SaveSpaceComponent, MessageComponent>();
            auto&& [e, save, m] = c.createAndGetEntityWithComponents<SaveSpaceComponent, MessageComponent>();
            save->mSpace = editor->get<const EditorSceneReferenceComponent>().mEditorScene;
            save->mToSave = editor->get<const EditorSavedSceneComponent>().mFilename;
            queue.mPendingMessage = e;
          }
          break;
        }
        case EditorPlayStateActionQueueComponent::ActionState::Update: {
          _awaitSpaceCompletion(*context, editor->get<const EditorSceneReferenceComponent>().mEditorScene, queue);
          break;
        }
      }
    }
  }

  void _loadSpace(PlayStateContext& context, const Entity& space, EditorPlayStateActionQueueComponent& queue) {
    if(auto editor = context.get<SceneRefView>().tryGetFirst()) {
      switch(queue.mState) {
        case EditorPlayStateActionQueueComponent::ActionState::Init: {
          if(auto cmd = context.get<GlobalCommandView>().tryGetFirst()) {
            auto c = cmd->get<GlobalCommandBufferComponent>().get<LoadSpaceComponent, MessageComponent>();
            auto&& [e, load, m] = c.createAndGetEntityWithComponents<LoadSpaceComponent, MessageComponent>();
            load->mSpace = space;
            load->mToLoad = editor->get<const EditorSavedSceneComponent>().mFilename;
            queue.mPendingMessage = e;
          }
          break;
        }
        case EditorPlayStateActionQueueComponent::ActionState::Update: {
          _awaitSpaceCompletion(context, space, queue);
          break;
        }
      }
    }
  }

  void _loadPlaySpaceAction(void* data, EditorPlayStateActionQueueComponent& queue) {
    auto* context = reinterpret_cast<PlayStateContext*>(data);
    if(auto editor = context->get<SceneRefView>().tryGetFirst()) {
      _loadSpace(*context, editor->get<const EditorSceneReferenceComponent>().mPlayScene, queue);
    }
  }

  void _loadEditorSpaceAction(void* data, EditorPlayStateActionQueueComponent& queue) {
    auto* context = reinterpret_cast<PlayStateContext*>(data);
    if(auto editor = context->get<SceneRefView>().tryGetFirst()) {
      _loadSpace(*context, editor->get<const EditorSceneReferenceComponent>().mEditorScene, queue);
    }
  }

  void tickPlayStateUpdate(PlayStateContext& context) {
    auto& view = context.get<PlayStateView>();
    if(view.begin() == view.end()) {
      return;
    }
    auto cmd = context.get<GlobalCommandView>().tryGetFirst();
    if(!cmd) {
      return;
    }
    auto c = cmd->get<GlobalCommandBufferComponent>().get<ImGuiContextComponent>();

    for(auto state : view) {
      EditorPlayStateComponent& s = state.get<EditorPlayStateComponent>();
      const bool stateChanged = s.mCurrentState != s.mLastState;
      EditorPlayState newState = s.mCurrentState;
      auto& actions = state.get<EditorPlayStateActionQueueComponent>();

      if(stateChanged) {
        switch(s.mCurrentState) {
          case EditorPlayState::Playing:
            //Remove imgui context from the editor entity, preventing all imgui rendering during play state
            if(s.mLastState == EditorPlayState::Stopped) {
              c.removeComponent<ImGuiContextComponent>(state.entity());
              actions.mActions.push(&_saveEditorSpaceAction);
              actions.mActions.push(&_clearPlaySpaceAction);
              actions.mActions.push(&_clearEditorSpaceAction);
              actions.mActions.push(&_loadPlaySpaceAction);
            }
            break;
          case EditorPlayState::Stopped:
            if(s.mLastState == EditorPlayState::Playing || s.mLastState == EditorPlayState::Invalid) {
              c.addComponent<ImGuiContextComponent>(state.entity());
              if(s.mLastState == EditorPlayState::Playing) {
                actions.mActions.push(&_clearEditorSpaceAction);
                actions.mActions.push(&_clearPlaySpaceAction);
                actions.mActions.push(&_loadEditorSpaceAction);
              }
            }
            break;
          case EditorPlayState::Paused:
            break;
          case EditorPlayState::Stepping:
            break;
          case EditorPlayState::Invalid:
            break;
        }
      }
      //No state changed
      else {
        switch(s.mCurrentState) {
          case EditorPlayState::Playing:
          case EditorPlayState::Stopped:
          case EditorPlayState::Paused:
          case EditorPlayState::Invalid:
            break;
          case EditorPlayState::Stepping:
              //If stepping and one step has elapsed, go back to paused
              if(s.mLastState == EditorPlayState::Stepping) {
                newState = EditorPlayState::Paused;
              }
              break;
        }
      }

      s.mLastState = s.mCurrentState;
      s.mCurrentState = newState;

      if(!actions.mActions.empty()) {
        const size_t beforeSize = actions.mActions.size();
        actions.mActions.front()(&context, actions);
        if(beforeSize != actions.mActions.size()) {
          actions.mState = EditorPlayStateActionQueueComponent::ActionState::Init;
        }
        else {
          actions.mState = EditorPlayStateActionQueueComponent::ActionState::Update;
        }
      }
    }
  }
}

std::shared_ptr<Engine::System> EditorSystem::sceneBrowser() {
  return ecx::makeSystem("SceneBrowser", &EditorImpl::tickSceneBrowser, IMGUI_THREAD);
}

std::shared_ptr<Engine::System> EditorSystem::toolbox() {
  return ecx::makeSystem("Toolbox", &EditorImpl::tickToolbox, IMGUI_THREAD);
}

std::shared_ptr<Engine::System> EditorSystem::playStateUpdate() {
  return ecx::makeSystem("PlayStateUpdate", &EditorImpl::tickPlayStateUpdate);
}

std::shared_ptr<Engine::System> EditorSystem::init() {
  using namespace Engine;
  using Commands = CommandBuffer<EditorContextComponent,
    EditorSavedSceneComponent,
    EditorSceneReferenceComponent,
    SpaceTagComponent,
    EditorPlayStateComponent,
    EditorPlayStateActionQueueComponent,
    DefaultPlaySpaceComponent
  >;

  return ecx::makeSystem("EditorInit", [](SystemContext<Commands>& context) {
    auto cmd = context.get<Commands>();

    auto&& [editorSpace, spaceComponent] = cmd.createAndGetEntityWithComponents<SpaceTagComponent>();
    auto&& [playSpace, sp, tag] = cmd.createAndGetEntityWithComponents<SpaceTagComponent, DefaultPlaySpaceComponent>();

    auto&& [contextEntity, editorContext, savedScene, sceneReference, playState, q] = cmd.createAndGetEntityWithComponents<
      EditorContextComponent,
      EditorSavedSceneComponent,
      EditorSceneReferenceComponent,
      EditorPlayStateComponent,
      EditorPlayStateActionQueueComponent>();
    sceneReference->mEditorScene = editorSpace;
    sceneReference->mPlayScene = playSpace;
    //Default to temp until explicitly changed
    savedScene->mFilename = "temp";
    playState->mCurrentState = EditorPlayState::Stopped;
    playState->mLastState = EditorPlayState::Invalid;
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
    const Entity editorSpace = editorContext->get<const EditorSceneReferenceComponent>().mEditorScene;

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

std::shared_ptr<Engine::System> EditorSystem::createPlatformListener() {
  return ecx::makeSystem("EditorPlatformListener", &EditorImpl::tickPlatformListener);
}

