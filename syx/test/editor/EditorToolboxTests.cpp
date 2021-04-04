#include "Precompile.h"
#include "CppUnitTest.h"

#include "App.h"
#include "editor/Editor.h"
#include "editor/event/EditorEvents.h"
#include "editor/MockEditorApp.h"
#include "event/EventBuffer.h"
#include "lua/LuaGameContext.h"
#include "system/KeyboardInput.h"
#include "test/MockApp.h"
#include "test/TestGUIHook.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EditorTests {
  void _assertToolboxExists(ITestGuiHook& gui) {
    MockEditorApp::findOrAssert(*MockEditorApp::getOrAssertQueryContext(gui), "Toolbox", [](const ITestGuiQueryContext& child) {
      MockEditorApp::findOrAssert(child, "Play", nullptr, L"Play button should exist");
    }, L"Toolbox window should have been found");
  }

  TEST_CLASS(ToolboxTests) {
    TEST_METHOD(EditorState_QueryScreen_ToolboxExists) {
      MockEditorApp app;
      Assert::IsNotNull(app->getSystem<Editor>(), L"Editor should exist", LINE_INFO());

      auto gui = Create::createAndRegisterTestGuiHook();
      app->update(0.f);

      _assertToolboxExists(*gui);
    }

    TEST_METHOD(PlayState_QueryScreen_NoGuiExists) {
      MockEditorApp app;
      Assert::IsNotNull(app->getSystem<Editor>(), L"Editor should exist", LINE_INFO());

      auto gui = Create::createAndRegisterTestGuiHook();
      app.pressButtonAndProcessInput("Play");

      Assert::IsNull(gui->query().get(), L"The editor scene tree should be empty in play mode");
    }

    TEST_METHOD(PlayState_ShiftF5_ToolboxExists) {
      MockEditorApp app;
      Assert::IsNotNull(app->getSystem<Editor>(), L"Editor should exist", LINE_INFO());
      auto gui = Create::createAndRegisterTestGuiHook();
      app.pressButtonAndProcessInput("Play");

      app.pressKeysAndProcessInput({ Key::F5, Key::Shift });

      _assertToolboxExists(*gui);
    }
  };
}