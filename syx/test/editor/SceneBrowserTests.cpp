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

    TEST_METHOD(NewlyCreatedObject_PressDeleteObject_ObjectIsDeleted) {
      TestApp app;

      app.pressButtonAndProcessInput(SceneBrowser::NEW_OBJECT_LABEL);
      app.pressButtonAndProcessInput(SceneBrowser::DELETE_OBJECT_LABEL);

      Assert::AreEqual(size_t(0), app.mRegistry.size<GameobjectComponent>());
    }

    TEST_METHOD(MockEditorApp_CreateObject_DoesNotAssert) {
      TestApp().createNewObject();
    }

    TEST_METHOD(TwoObjectsFirstSelected_PressDeleteObject_FirstIsDeleted) {
      TestApp app;
      const auto firstHandle = app.createNewObject();
      const auto secondHandle = app.createNewObject();

      app.setAndUpdateSelection({ firstHandle });
      app.pressButtonAndProcessInput(SceneBrowser::DELETE_OBJECT_LABEL);

      Assert::IsFalse(app.mRegistry.isValid(firstHandle), L"First object should have been deleted when the delete button was pressed", LINE_INFO());
      Assert::IsTrue(app.mRegistry.isValid(secondHandle), L"Second object should remain because it was not selected when delete was pressed", LINE_INFO());
    }

    TEST_METHOD(TwoObjectsBothSelected_PressDeleteObject_AllDeleted) {
      TestApp app;
      std::vector<Engine::Entity> allObjects;
      for(int i = 0; i < 2; ++i) {
        allObjects.push_back(app.createNewObject());
      }
      app.setAndUpdateSelection(std::move(allObjects));

      app.pressButtonAndProcessInput(SceneBrowser::DELETE_OBJECT_LABEL);

      Assert::AreEqual(size_t(0), app.mRegistry.size<GameobjectComponent>());
    }

    /* TODO:
    TEST_METHOD(SingleObject_PickWithMouse_IsSelected) {
      TestApp app;
      const std::vector<size_t> expectedSelection { app.createNewObject().getHandle() };

      const std::vector<Handle> newSelection = app.simulateMousePick(expectedSelection);

      Assert::IsTrue(expectedSelection == newSelection, L"Picked objects should have been selected", LINE_INFO());
    }
    */

    TEST_METHOD(SceneBrowser_QueryObjectsWindow_HasStaticButtons) {
      TestApp app;
      auto gui = Create::createAndRegisterTestGuiHook();
      app.update();

      MockEditorApp::findOrAssert(*MockEditorApp::getOrAssertQueryContext(*gui), SceneBrowser::WINDOW_NAME, [](const ITestGuiQueryContext& child) {
        MockEditorApp::findOrAssert(child, SceneBrowser::NEW_OBJECT_LABEL, nullptr, L"New object button should exist");
        MockEditorApp::findOrAssert(child, SceneBrowser::DELETE_OBJECT_LABEL, nullptr, L"Delete object button should exist");
      }, L"Objects window should have been found");
    }

    TEST_METHOD(SceneBrowserOneObject_QueryObjectList_IsInObjectList) {
      TestApp app;
      const std::string objectName = "name";
      app.createNewObjectWithName(objectName);
      auto gui = Create::createAndRegisterTestGuiHook();
      app.update();

      MockEditorApp::findOrAssert(*MockEditorApp::getOrAssertQueryContext(*gui), SceneBrowser::WINDOW_NAME, [&objectName](const ITestGuiQueryContext& objects) {
        MockEditorApp::findContainsOrAssert(objects, SceneBrowser::OBJECT_LIST_NAME, [&objectName](const ITestGuiQueryContext& scrollView) {
          MockEditorApp::findContainsOrAssert(scrollView, objectName, [](const ITestGuiQueryContext& objectEntry) {
            Assert::IsTrue(objectEntry.getData().is<TestGuiElementData::Button>(), L"Object element in list should be a button");
          }, L"Created object should be in the object list");
        }, L"Objects scroll view should exist");
      }, L"Objects window should have been found");
    }

    TEST_METHOD(SceneBrowserTwoObjects_QueryObjectList_SortedByEntityID) {
      TestApp app;
      const std::string objectName = "name";
      app.createNewObjectWithName(objectName);
      const std::string secondName = objectName + "second";
      app.createNewObjectWithName(secondName);
      auto gui = Create::createAndRegisterTestGuiHook();
      app.update();

      MockEditorApp::findOrAssert(*MockEditorApp::getOrAssertQueryContext(*gui), SceneBrowser::WINDOW_NAME, [&](const ITestGuiQueryContext& objects) {
        MockEditorApp::findContainsOrAssert(objects, SceneBrowser::OBJECT_LIST_NAME, [&](const ITestGuiQueryContext& scrollView) {
          std::vector<std::string> orderedElements;
          MockEditorApp::findAllContainsOrAssert(2, scrollView, objectName, [&](const ITestGuiQueryContext& objectEntry) {
            orderedElements.push_back(objectEntry.getName());
          }, L"Created object should be in the object list");
          Assert::IsTrue(orderedElements[1].find(secondName) != std::string::npos);
        }, L"Objects scroll view should exist");
      }, L"Objects window should have been found");
    }

    //Selecting an object moves it between chunks in the registry but ui order should be preserved
    TEST_METHOD(SceneBrowserTwoObjects_SelectThenQueryObjectList_SortedByEntityID) {
      TestApp app;
      const std::string objectName = "name";
      auto firstObj = app.createNewObjectWithName(objectName);
      const std::string secondName = objectName + "second";
      app.createNewObjectWithName(secondName);
      auto gui = Create::createAndRegisterTestGuiHook();
      app.setAndUpdateSelection({ firstObj });
      app.update();

      MockEditorApp::findOrAssert(*MockEditorApp::getOrAssertQueryContext(*gui), SceneBrowser::WINDOW_NAME, [&](const ITestGuiQueryContext& objects) {
        MockEditorApp::findContainsOrAssert(objects, SceneBrowser::OBJECT_LIST_NAME, [&](const ITestGuiQueryContext& scrollView) {
          std::vector<std::string> orderedElements;
          MockEditorApp::findAllContainsOrAssert(2, scrollView, objectName, [&](const ITestGuiQueryContext& objectEntry) {
            orderedElements.push_back(objectEntry.getName());
          }, L"Created object should be in the object list");
          Assert::IsTrue(orderedElements[1].find(secondName) != std::string::npos);
        }, L"Objects scroll view should exist");
      }, L"Objects window should have been found");
    }
  };
}