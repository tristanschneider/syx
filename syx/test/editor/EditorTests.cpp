#include "Precompile.h"
#include "CppUnitTest.h"

#include "App.h"
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
#include "test/TestListenerSystem.h"
#include "threading/AsyncHandle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EditorTests {
  const std::string NEW_OBJECT_BUTTON = "New Object";
  const std::string DELETE_OBJECT_BUTTON = "Delete Object";

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
  };
}