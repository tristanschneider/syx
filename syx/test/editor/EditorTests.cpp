#include "Precompile.h"
#include "CppUnitTest.h"

#include "App.h"
#include "Camera.h"
#include "editor/Editor.h"
#include "editor/event/EditorEvents.h"
#include "event/BaseComponentEvents.h"
#include "event/SpaceEvents.h"
#include "event/EventBuffer.h"
#include "LuaGameObject.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaState.h"
#include "lua/LuaVariant.h"
#include "system/AssetRepo.h"
#include "system/LuaGameSystem.h"
#include "SyxVec2.h"
#include "test/MockApp.h"
#include "test/TestAppPlatform.h"
#include "test/TestAppRegistration.h"
#include "test/TestFileSystem.h"
#include "test/TestGUIHook.h"
#include "test/TestKeyboardInput.h"
#include "test/TestListenerSystem.h"
#include "threading/AsyncHandle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EditorTests {
  const std::string NEW_OBJECT_BUTTON = "New Object";
  const std::string DELETE_OBJECT_BUTTON = "Delete Object";
  const std::string OBJECTS_WINDOW = "Objects";
  const std::string OBJECTS_SCROLL_VIEW = "ScrollView";

  TEST_CLASS(BaseEditorTests) {
    struct MockEditorApp : public MockApp {
      using ScopedAssertion = FinalAction<std::function<void()>>;

      MockEditorApp()
        : MockApp(std::make_unique<TestAppPlatform>(), TestRegistration::createEditorRegistration()) {
      }

      void processEditorInput() {
        //The first update will process whatever the desired input was, the second will process whatever event the input sent
        mApp->update(0.f);
        mApp->update(0.f);
      }

      void pressButtonAndProcessInput(const std::string& label) {
        {
          auto gui = Create::createAndRegisterTestGuiHook();
          auto press = gui->addScopedButtonPress(label);
          mApp->update(0.f);
        }
        mApp->update(0.f);
      }

      //Set a selection and propagate the change to the editor
      void setAndUpdateSelection(std::vector<Handle> selection) {
        mApp->getMessageQueue()->push(SetSelectionEvent(std::move(selection)));
        //Editor update happens to be before editor messages are processed, so if the test wants to trigger logic in the editor udpate that depends on the new selection, wait a frame
        mApp->update(0.f);
      }

      ScopedAssertion createScopedNetObjectCountAssertion(int netChange, const std::wstring& assertMessage) {
        const size_t prevCount = mApp->getSystem<LuaGameSystem>()->getObjects().size();
        const size_t expectedCount = static_cast<size_t>(static_cast<int>(prevCount) + netChange);
        return finally(std::function<void()>([expectedCount, assertMessage, this] {
          Assert::AreEqual(expectedCount, mApp->getSystem<LuaGameSystem>()->getObjects().size(), assertMessage.c_str(), LINE_INFO());
        }));
      }

      const LuaGameObject& createNewObject() {
        //Simulate creating the object through the editor
        pressButtonAndProcessInput(NEW_OBJECT_BUTTON);

        const Event* createEvent = mApp->getSystem<TestListenerSystem>()->tryGetEventOfType(Event::typeId<AddGameObjectEvent>());
        Assert::IsNotNull(createEvent, L"Creation event should be found when a new object is created");
        //New object creation is expected to use this field, not the IClaimedUniqueID since there's no known value to claim
        const Handle newHandle = static_cast<const AddGameObjectEvent&>(*createEvent).mObj;

        const LuaGameObject* result = mApp->getSystem<LuaGameSystem>()->getObject(newHandle);
        Assert::IsNotNull(result, L"New object should have been found in LuaGameSystem, if it wasn't the system failed to created it or the id from the add event didn't match");
        return *result;
      }

      //Arbitrary camera values that result in Camera::isValid returning true
      static Camera _createValidCamera() {
        Camera result(CameraOps(1.f, 1.f, 1.f, 2.f, 0));
        result.setViewport("arbitrary");
        Assert::IsTrue(result.isValid(), L"Values above should be considered valid, if not, this helper needs to be adjusted", LINE_INFO());
        return result;
      }

      //One update to process the event and another for the response to be processed
      void _updateForEventResponse() {
        for(int i = 0; i < 2; ++i) {
          mApp->update(0.f);
        }
      }

      std::vector<Handle> simulateMousePick(const std::vector<Handle> objs) {
        auto& input = static_cast<TestKeyboardInputImpl&>(mApp->getAppPlatform().getKeyboardInput());
        //Needs to be down for a frame then released to trigger the pick behavior
        input.clearInputAfterOneFrame().mKeyStates[Key::LeftMouse] = KeyState::Triggered;
        mApp->update(1.f);
        input.clearInputAfterOneFrame().mKeyStates[Key::LeftMouse] = KeyState::Released;

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
        Assert::IsTrue(listener.hasEventOfType(Event::typeId<GetCameraRequest>()), L"Mouse input should have triggered the GetCameraRequest to bein the pick process", LINE_INFO());
        _updateForEventResponse();
        Assert::IsTrue(listener.hasEventOfType(Event::typeId<ScreenPickRequest>()), L"GetCameraResponse should have triggered the ScreenPickRequest", LINE_INFO());
        _updateForEventResponse();
        auto* selection = static_cast<const SetSelectionEvent*>(listener.tryGetEventOfType(Event::typeId<SetSelectionEvent>()));
        Assert::IsNotNull(selection, L"SetSelectionEvent should have been triggered by the ScreenPickResponse", LINE_INFO());

        return selection->mObjects;
      }

      std::vector<Handle> simulateMousePick(Handle obj) {
        return simulateMousePick(std::vector{ obj });
      }
    };

    std::unique_ptr<ILuaGameContext> _createGameContext(MockApp& app) {
      return Lua::createGameContext(*app->getSystem<LuaGameSystem>());
    }

    const LuaGameObject* _getObject(const UniqueID& id, MockApp& app) {
      const auto& objs = app->getSystem<LuaGameSystem>()->getObjects();
      const auto it = std::find_if(objs.begin(), objs.end(), [&id](const auto& pair) {
        return pair.second->getUniqueID() == id;
      });
      return it != objs.end() ? it->second.get() : nullptr;
    }

    TEST_METHOD(Editor_Update_NothingHappens) {
      MockEditorApp app;
      Assert::IsNotNull(app->getSystem<Editor>(), L"Editor should exist", LINE_INFO());

      app->update(0.f);
    }

    TEST_METHOD(EmptyScene_PressNewObject_ObjectIsAdded) {
      MockEditorApp app;
      auto assertion = app.createScopedNetObjectCountAssertion(1, L"A new object should have been created");

      app.pressButtonAndProcessInput(NEW_OBJECT_BUTTON);

      Assert::IsTrue(app->getSystem<TestListenerSystem>()->hasEventOfType(Event::typeId<AddGameObjectEvent>()), L"Pressing new object should have sent an AddGameObjectEvent");
    }

    TEST_METHOD(NewlyCreatedObject_PressDeleteObject_ObjectIsDeleted) {
      MockEditorApp app;
      auto net = app.createScopedNetObjectCountAssertion(0, L"In the end the object count should be the same because an object was added and removed");
      {
        auto button = app.createScopedNetObjectCountAssertion(1, L"The add button should have created a new object");
        //This also selects the newly created object, so pressing delete immediately will destroy the newly created object
        app.pressButtonAndProcessInput(NEW_OBJECT_BUTTON);
      }

      app.pressButtonAndProcessInput(DELETE_OBJECT_BUTTON);
      Assert::IsTrue(app->getSystem<TestListenerSystem>()->hasEventOfType(Event::typeId<RemoveGameObjectEvent>()), L"Pressing delete object should have sent the corresponding message");
    }

    TEST_METHOD(MockEditorApp_CreateObject_DoesNotAssert) {
      MockEditorApp().createNewObject();
    }

    TEST_METHOD(TwoObjectsFirstSelected_PressDeleteObject_FirstIsDeleted) {
      MockEditorApp app;
      auto net = app.createScopedNetObjectCountAssertion(1, L"1 should remain after adding 2 and removing 1");
      const Handle firstHandle = app.createNewObject().getHandle();
      const Handle secondHandle = app.createNewObject().getHandle();

      app.setAndUpdateSelection(std::vector<size_t>{ firstHandle });
      app.pressButtonAndProcessInput(DELETE_OBJECT_BUTTON);

      LuaGameSystem* game = app->getSystem<LuaGameSystem>();
      Assert::IsNull(game->getObject(firstHandle), L"First object should have been deleted when the delete button was pressed", LINE_INFO());
      Assert::IsNotNull(game->getObject(secondHandle), L"Second object should remain because it was not selected when delete was pressed", LINE_INFO());
    }

    TEST_METHOD(TwoObjectsBothSelected_PressDeleteObject_AllDeleted) {
      MockEditorApp app;
      auto net = app.createScopedNetObjectCountAssertion(0, L"All selected objects should have been destroyed upon pressing the destroy button");
      std::vector<Handle> allObjects;
      for(int i = 0; i < 2; ++i) {
        allObjects.push_back(app.createNewObject().getHandle());
      }
      app.setAndUpdateSelection(std::move(allObjects));

      app.pressButtonAndProcessInput(DELETE_OBJECT_BUTTON);

      //Scope exits here, asserting that everything was deleted
    }

    TEST_METHOD(SingleObject_PickWithMouse_IsSelected) {
      MockEditorApp app;
      const std::vector<size_t> expectedSelection { app.createNewObject().getHandle() };

      const std::vector<Handle> newSelection = app.simulateMousePick(expectedSelection);

      Assert::IsTrue(expectedSelection == newSelection, L"Picked objects should have been selected", LINE_INFO());
    }

    static std::shared_ptr<ITestGuiQueryContext> _getOrAssertQueryContext(ITestGuiHook& hook) {
      auto result = hook.query();
      Assert::IsTrue(result != nullptr, L"Root should exist", LINE_INFO());
      return result;
    }

    //Find a shallow child node who triggers a false return of the callback
    static void _invokeOrAssert(const ITestGuiQueryContext& query, const std::function<bool(const ITestGuiQueryContext&)> callback, const std::wstring& assertMsg) {
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
    static void _findOrAssert(const ITestGuiQueryContext& query, const std::string& name, const std::function<void(const ITestGuiQueryContext&)> callback, const std::wstring& assertMsg) {
      _invokeOrAssert(query, [&name, &callback](const ITestGuiQueryContext& child) {
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
    static void _findContainsOrAssert(const ITestGuiQueryContext& query, const std::string& name, const std::function<void(const ITestGuiQueryContext&)> callback, const std::wstring& assertMsg) {
      _invokeOrAssert(query, [&name, &callback](const ITestGuiQueryContext& child) {
        if(child.getName().find(name) != std::string::npos) {
          if(callback) {
            callback(child);
          }
          return false;
        }
        return true;
      }, assertMsg);
    }

    TEST_METHOD(SceneBrowser_QueryObjectsWindow_HasStaticButtons) {
      MockEditorApp app;
      auto gui = Create::createAndRegisterTestGuiHook();
      app->update(0.f);

      _findOrAssert(*_getOrAssertQueryContext(*gui), OBJECTS_WINDOW, [](const ITestGuiQueryContext& child) {
        _findOrAssert(child, NEW_OBJECT_BUTTON, nullptr, L"New object button should exist");
        _findOrAssert(child, DELETE_OBJECT_BUTTON, nullptr, L"Delete object button should exist");
      }, L"Objects window should have been found");
    }

    TEST_METHOD(SceneBrowserOneObject_QueryObjectList_IsInObjectList) {
      MockEditorApp app;
      const std::string objectName(app.createNewObject().getName().getName());
      auto gui = Create::createAndRegisterTestGuiHook();
      app->update(0.f);

      _findOrAssert(*_getOrAssertQueryContext(*gui), OBJECTS_WINDOW, [&objectName](const ITestGuiQueryContext& objects) {
        _findContainsOrAssert(objects, OBJECTS_SCROLL_VIEW, [&objectName](const ITestGuiQueryContext& scrollView) {
          _findContainsOrAssert(scrollView, objectName, [](const ITestGuiQueryContext& objectEntry) {
            Assert::IsTrue(objectEntry.getType() == TestGuiElementType::Button, L"Object element in list should be a button");
          }, L"Created object should be in the object list");
        }, L"Objects scroll view should exist");
      }, L"Objects window should have been found");
    }
  };
}