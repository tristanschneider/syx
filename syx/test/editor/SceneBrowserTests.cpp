#include "Precompile.h"
#include "CppUnitTest.h"

#include "App.h"
#include "Camera.h"
#include "editor/Editor.h"
#include "editor/event/EditorEvents.h"
#include "editor/MockEditorApp.h"
#include "editor/SceneBrowser.h"
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

#include "ecs/component/GameobjectComponent.h"

namespace EditorTests {
  TEST_CLASS(SceneBrowserTests) {
    TEST_METHOD(EmptyScene_PressNewObject_ObjectIsAdded) {
      TestApp app;

      app.pressButtonAndProcessInput(SceneBrowser::NEW_OBJECT_LABEL);

      Assert::AreEqual(size_t(1), app.mRegistry.size<GameobjectComponent>());
    }
    /* TODO: fix
    TEST_METHOD(NewlyCreatedObject_PressDeleteObject_ObjectIsDeleted) {
      MockEditorApp app;
      auto net = app.createScopedNetObjectCountAssertion(0, L"In the end the object count should be the same because an object was added and removed");
      {
        auto button = app.createScopedNetObjectCountAssertion(1, L"The add button should have created a new object");
        //This also selects the newly created object, so pressing delete immediately will destroy the newly created object
        app.pressButtonAndProcessInput(SceneBrowser::NEW_OBJECT_LABEL);
      }

      app.pressButtonAndProcessInput(SceneBrowser::DELETE_OBJECT_LABEL);
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
      app.pressButtonAndProcessInput(SceneBrowser::DELETE_OBJECT_LABEL);

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

      app.pressButtonAndProcessInput(SceneBrowser::DELETE_OBJECT_LABEL);

      //Scope exits here, asserting that everything was deleted
    }

    TEST_METHOD(SingleObject_PickWithMouse_IsSelected) {
      MockEditorApp app;
      const std::vector<size_t> expectedSelection { app.createNewObject().getHandle() };

      const std::vector<Handle> newSelection = app.simulateMousePick(expectedSelection);

      Assert::IsTrue(expectedSelection == newSelection, L"Picked objects should have been selected", LINE_INFO());
    }

    TEST_METHOD(SceneBrowser_QueryObjectsWindow_HasStaticButtons) {
      MockEditorApp app;
      auto gui = Create::createAndRegisterTestGuiHook();
      app->update(0.f);

      MockEditorApp::findOrAssert(*MockEditorApp::getOrAssertQueryContext(*gui), SceneBrowser::WINDOW_NAME, [](const ITestGuiQueryContext& child) {
        MockEditorApp::findOrAssert(child, SceneBrowser::NEW_OBJECT_LABEL, nullptr, L"New object button should exist");
        MockEditorApp::findOrAssert(child, SceneBrowser::DELETE_OBJECT_LABEL, nullptr, L"Delete object button should exist");
      }, L"Objects window should have been found");
    }

    TEST_METHOD(SceneBrowserOneObject_QueryObjectList_IsInObjectList) {
      MockEditorApp app;
      const std::string objectName(app.createNewObject().getName().getName());
      auto gui = Create::createAndRegisterTestGuiHook();
      app->update(0.f);

      MockEditorApp::findOrAssert(*MockEditorApp::getOrAssertQueryContext(*gui), SceneBrowser::WINDOW_NAME, [&objectName](const ITestGuiQueryContext& objects) {
        MockEditorApp::findContainsOrAssert(objects, SceneBrowser::OBJECT_LIST_NAME, [&objectName](const ITestGuiQueryContext& scrollView) {
          MockEditorApp::findContainsOrAssert(scrollView, objectName, [](const ITestGuiQueryContext& objectEntry) {
            Assert::IsTrue(objectEntry.getData().is<TestGuiElementData::Button>(), L"Object element in list should be a button");
          }, L"Created object should be in the object list");
        }, L"Objects scroll view should exist");
      }, L"Objects window should have been found");
    }
    */
  };
}