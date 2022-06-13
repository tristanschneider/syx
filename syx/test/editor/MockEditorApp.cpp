#include "Precompile.h"
#include "editor/MockEditorApp.h"

#include "App.h"
#include "Camera.h"
#include "CppUnitTest.h"
#include "ecs/component/EditorComponents.h"
#include "ecs/component/GameobjectComponent.h"
#include "editor/event/EditorEvents.h"
#include "editor/ObjectInspector.h"
#include "editor/SceneBrowser.h"
#include "event/BaseComponentEvents.h"
#include "event/InputEvents.h"
#include "LuaGameObject.h"
#include "system/LuaGameSystem.h"
#include "test/TestAppPlatform.h"
#include "test/TestAppRegistration.h"
#include "test/TestGUIHook.h"
#include "test/TestImGuiSystem.h"
#include "test/TestListenerSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EditorTests {
  TestApp::TestApp() {
    Registration::createDefaultApp()->registerAppContext(mContext);

    Engine::AppContext::PhaseContainer initializers = mContext.getInitializers();
    initializers.mSystems.push_back(TestImGuiSystem::init());

    Engine::AppContext::PhaseContainer input = mContext.getUpdatePhase(Engine::AppPhase::Input);
    input.mSystems.insert(input.mSystems.begin(), TestImGuiSystem::update());

    mContext.registerInitializer(std::move(initializers.mSystems));
    mContext.registerUpdatePhase(Engine::AppPhase::Input, std::move(input.mSystems), input.mTargetFPS);

    mContext.buildExecutionGraph();

    mContext.initialize(mRegistry);
  }

  void TestApp::pressButtonAndProcessInput(const std::string& label) {
    auto gui = Create::createAndRegisterTestGuiHook();
    auto press = gui->addScopedButtonPress(label);
    update();
  }

  void TestApp::update() {
    mContext.addTickToAllPhases();
    mContext.update(mRegistry, size_t(1));
  }

  Engine::Entity TestApp::createNewObject() {
    return mRegistry.createEntityWithComponents<GameobjectComponent>(*mRegistry.getDefaultEntityGenerator());
  }

  Engine::Entity TestApp::createNewObjectWithName(std::string name) {
    auto&& [entity, a, nameTag ] = mRegistry.createAndGetEntityWithComponents<GameobjectComponent, NameTagComponent>(*mRegistry.getDefaultEntityGenerator());
    nameTag.get().mName = std::move(name);
    return entity;
  }

  void TestApp::addComponentFromUI(const std::string& name) {
    auto gui = Create::createAndRegisterTestGuiHook();
    auto addComponent = gui->addScopedButtonPress(ObjectInspector::ADD_COMPONENT_BUTTON);
    update();
    auto pick = gui->addScopedPickerPick(name);
    update();
  }

  void TestApp::setAndUpdateSelection(const std::vector<Engine::Entity>& entities) {
    mRegistry.removeComponentsFromAllEntities<SelectedComponent>();
    for(const Engine::Entity& entity : entities) {
      mRegistry.addComponent<SelectedComponent>(entity);
    }
  }

  MockEditorApp::MockEditorApp()
    : MockApp(std::make_unique<TestAppPlatform>(), TestRegistration::createEditorRegistration()) {
  }

  MockEditorApp::~MockEditorApp() = default;

  void MockEditorApp::processEditorInput() {
    //The first update will process whatever the desired input was, the second will process whatever event the input sent
    mApp->update(0.f);
    mApp->update(0.f);
  }

  void MockEditorApp::pressButtonAndProcessInput(const std::string& label) {
    {
      auto gui = Create::createAndRegisterTestGuiHook();
      auto press = gui->addScopedButtonPress(label);
      mApp->update(0.f);
    }
    mApp->update(0.f);
  }

  void MockEditorApp::pressKeyAndProcessInput(Key key) {
    pressKeysAndProcessInput({ key });
  }

  void MockEditorApp::pressKeysAndProcessInput(const std::initializer_list<Key>& keys) {
    auto& events = *mApp->getMessageQueue();
    for(auto key : keys) {
      events.push(KeyEvent(key, KeyState::Triggered));
    }
    mApp->update(1.f);
    for(auto key : keys) {
      events.push(KeyEvent(key, KeyState::Released));
    }
    mApp->update(1.f);
  }

  //Set a selection and propagate the change to the editor
  void MockEditorApp::setAndUpdateSelection(std::vector<Handle> selection) {
    mApp->getMessageQueue()->push(SetSelectionEvent(std::move(selection)));
    //Editor update happens to be before editor messages are processed, so if the test wants to trigger logic in the editor udpate that depends on the new selection, wait a frame
    mApp->update(0.f);
  }

  MockEditorApp::ScopedAssertion MockEditorApp::createScopedNetObjectCountAssertion(int netChange, const std::wstring& assertMessage) {
    const size_t prevCount = mApp->getSystem<LuaGameSystem>()->getObjects().size();
    const size_t expectedCount = static_cast<size_t>(static_cast<int>(prevCount) + netChange);
    return finally(std::function<void()>([expectedCount, assertMessage, this] {
      Assert::AreEqual(expectedCount, mApp->getSystem<LuaGameSystem>()->getObjects().size(), assertMessage.c_str(), LINE_INFO());
    }));
  }

  const LuaGameObject& MockEditorApp::createNewObject() {
    //Simulate creating the object through the editor
    pressButtonAndProcessInput(SceneBrowser::NEW_OBJECT_LABEL);

    const Event* createEvent = mApp->getSystem<TestListenerSystem>()->tryGetEventOfType(Event::typeId<AddGameObjectEvent>());
    Assert::IsNotNull(createEvent, L"Creation event should be found when a new object is created");
    //New object creation is expected to use this field, not the IClaimedUniqueID since there's no known value to claim
    const Handle newHandle = static_cast<const AddGameObjectEvent&>(*createEvent).mObj;

    const LuaGameObject* result = mApp->getSystem<LuaGameSystem>()->getObject(newHandle);
    Assert::IsNotNull(result, L"New object should have been found in LuaGameSystem, if it wasn't the system failed to created it or the id from the add event didn't match");
    return *result;
  }

  const LuaGameObject& MockEditorApp::findGameObjectOrAssert(const std::string_view name) {
    LuaGameSystem* game = mApp->getSystem<LuaGameSystem>();
    Assert::IsNotNull(game, L"Game should exist");
    auto it = std::find_if(game->getObjects().begin(), game->getObjects().end(), [&name](const auto& pair) {
      LuaGameObject& obj = *pair.second;
      if(const NameComponent* nameComponent = obj.getComponent<NameComponent>()) {
        return nameComponent->getName() == name;
      }
      return false;
    });
    Assert::IsFalse(it == game->getObjects().end(), L"Object should have been found");
    return *it->second;
  }

  const Component& MockEditorApp::createComponent(const LuaGameObject& object, const ComponentTypeInfo& type) {
      auto gui = Create::createAndRegisterTestGuiHook();
      auto addComponent = gui->addScopedButtonPress(ObjectInspector::ADD_COMPONENT_BUTTON);
      mApp->update(1.f);
      auto pick = gui->addScopedPickerPick(type.mTypeName);
      processEditorInput();

      const Component* result = object.getComponent(type.mPropName.c_str());
      Assert::IsNotNull(result, L"Component should have been added after picking it in the new component picker");
      return *result;
  }

  //Arbitrary camera values that result in Camera::isValid returning true
  Camera MockEditorApp::_createValidCamera() {
    Camera result(CameraOps(1.f, 1.f, 1.f, 2.f, 0));
    result.setViewport("arbitrary");
    Assert::IsTrue(result.isValid(), L"Values above should be considered valid, if not, this helper needs to be adjusted", LINE_INFO());
    return result;
  }

  //One update to process the event and another for the response to be processed
  void MockEditorApp::_updateForEventResponse() {
    for(int i = 0; i < 2; ++i) {
      mApp->update(0.f);
    }
  }

  std::vector<Handle> MockEditorApp::simulateMousePick(const std::vector<Handle>& objs) {
    //Needs to be down for a frame then released to trigger the pick behavior
    pressKeyAndProcessInput(Key::LeftMouse);

    //SceneBrowser gets the camera with a GetCameraRequest, then uses it to send a ScreenPickRequest
    //Those are normally provided by the GraphicsSystem. These tests don't have that system, so mock the responses
    TestListenerSystem& listener = *mApp->getSystem<TestListenerSystem>();
    listener.registerEventHandler(Event::typeId<GetCameraRequest>(), TestListenerSystem::HandlerLifetime::SingleUse, TestListenerSystem::HandlerResponse::Continue, [](const Event& e, MessageQueueProvider& msg) {
      //Details of mouse and camera aren't needed since this isn't testing the coordinate logic
      static_cast<const GetCameraRequest&>(e).respond(*msg.getMessageQueue(), GetCameraResponse(_createValidCamera()));
    })
    .registerEventHandler(Event::typeId<ScreenPickRequest>(), TestListenerSystem::HandlerLifetime::SingleUse, TestListenerSystem::HandlerResponse::Continue, [objs(objs)](const Event& e, MessageQueueProvider& msg) mutable {
      auto req = static_cast<const ScreenPickRequest&>(e);
      req.respond(*msg.getMessageQueue(), ScreenPickResponse(req.mRequestId, req.mSpace, std::move(objs)));
    });

    _updateForEventResponse();
    Assert::IsTrue(listener.hasEventOfType(Event::typeId<GetCameraRequest>()), L"Mouse input should have triggered the GetCameraRequest to begin the pick process", LINE_INFO());
    _updateForEventResponse();
    Assert::IsTrue(listener.hasEventOfType(Event::typeId<ScreenPickRequest>()), L"GetCameraResponse should have triggered the ScreenPickRequest", LINE_INFO());
    _updateForEventResponse();
    auto* selection = static_cast<const SetSelectionEvent*>(listener.tryGetEventOfType(Event::typeId<SetSelectionEvent>()));
    for(int i = 0; i < 3 && !selection; ++i) {
      //Selection is deferred, try a few frames
      mApp->update(0.f);
      selection = static_cast<const SetSelectionEvent*>(listener.tryGetEventOfType(Event::typeId<SetSelectionEvent>()));
    }

    Assert::IsNotNull(selection, L"SetSelectionEvent should have been triggered by the ScreenPickResponse", LINE_INFO());

    return selection->mObjects;
  }

  std::vector<Handle> MockEditorApp::simulateMousePick(Handle obj) {
    return simulateMousePick(std::vector{ obj });
  }

  std::shared_ptr<ITestGuiQueryContext> MockEditorApp::getOrAssertQueryContext(ITestGuiHook& hook) {
    auto result = hook.query();
    Assert::IsTrue(result != nullptr, L"Root should exist", LINE_INFO());
    return result;
  }

  //Find a shallow child node who triggers a false return of the callback
  void MockEditorApp::invokeOrAssert(const ITestGuiQueryContext& query, const std::function<bool(const ITestGuiQueryContext&)> callback, const std::wstring& assertMsg) {
    bool invoked = false;
    query.visitChildrenShallow([callback, &invoked](const ITestGuiQueryContext& child) {
      bool shouldContinue = false;
      if(callback) {
        shouldContinue = callback(child);
      }
      //Assume that the callback will return false (don't continue) when it has found what it's looking for
      invoked = !shouldContinue;
      return shouldContinue;
    });
    Assert::IsTrue(invoked, assertMsg.c_str(), LINE_INFO());
  }

  //Find a shallow child node whose name matches the given string
  void MockEditorApp::findOrAssert(const ITestGuiQueryContext& query, const std::string& name, const std::function<void(const ITestGuiQueryContext&)> callback, const std::wstring& assertMsg) {
    invokeOrAssert(query, [&name, &callback](const ITestGuiQueryContext& child) {
      if(child.getName() == name) {
        if (callback) {
          callback(child);
        }
        return false;
      }
      return true;
    }, assertMsg);
  }

  //Find a shallow child node whose name contains the given string
  void MockEditorApp::findContainsOrAssert(const ITestGuiQueryContext& query, const std::string& name, const std::function<void(const ITestGuiQueryContext&)> callback, const std::wstring& assertMsg) {
    invokeOrAssert(query, [&name, &callback](const ITestGuiQueryContext& child) {
      if(child.getName().find(name) != std::string::npos) {
        if(callback) {
          callback(child);
        }
        return false;
      }
      return true;
    }, assertMsg);
  }

  void MockEditorApp::findAllContainsOrAssert(size_t expectedCount, const ITestGuiQueryContext& query, const std::string& name, const std::function<void(const ITestGuiQueryContext&)> callback, const std::wstring& assertMsg) {
    size_t actualCount = 0;
    query.visitChildrenShallow([&actualCount, &name, &callback](const ITestGuiQueryContext& child) {
      if(child.getName().find(name) != std::string::npos) {
        ++actualCount;
        if(callback) {
          callback(child);
        }
      }
      return true;
    });
    Assert::AreEqual(expectedCount, actualCount, assertMsg.c_str());
  }

}