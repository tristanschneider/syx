#include "Precompile.h"
#include "CppUnitTest.h"

#include "ecs/component/EditorComponents.h"
#include "ecs/component/GameobjectComponent.h"
#include "ecs/component/ImGuiContextComponent.h"
#include "ecs/component/RawInputComponent.h"
#include "editor/MockEditorApp.h"
#include "test/TestGUIHook.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EditorTests {
  void _assertToolboxExists(ITestGuiHook& gui) {
    MockEditorApp::findOrAssert(*MockEditorApp::getOrAssertQueryContext(gui), "Toolbox", [](const ITestGuiQueryContext& child) {
      MockEditorApp::findOrAssert(child, "Play", nullptr, L"Play button should exist");
    }, L"Toolbox window should have been found");
  }

  void _assertPlayStateMatches(TestApp& app, EditorPlayState state) {
    auto it = app.mRegistry.begin<EditorPlayStateComponent>();
    Assert::IsFalse(it == app.mRegistry.end<EditorPlayStateComponent>());
    Assert::IsTrue(state == it->mCurrentState);
  }

  TEST_CLASS(ToolboxTests) {
    TEST_METHOD(EditorState_QueryScreen_ToolboxExists) {
      TestApp app;
      auto gui = Create::createAndRegisterTestGuiHook();

      app.update();

      _assertToolboxExists(*gui);
    }

    TEST_METHOD(PlayState_QueryScreen_NoGuiExists) {
      TestApp app;

      auto gui = Create::createAndRegisterTestGuiHook();
      auto press = gui->addScopedButtonPress("Play");
      app.update();

      Assert::AreEqual(size_t(0), app.mRegistry.size<ImGuiContextComponent>(), L"Imgui context should be removed");
    }

    TEST_METHOD(PlayState_ShiftF5_ToolboxExists) {
      TestApp app;
      app.pressButtonAndProcessInput("Play");
      _assertPlayStateMatches(app, EditorPlayState::Playing);

      app.pressKeysAndProcessInput({ Key::F5, Key::Shift });
      auto gui = Create::createAndRegisterTestGuiHook();
      //One update to process the play state change, another to populate the editor ui
      app.update();

      _assertToolboxExists(*gui);
      _assertPlayStateMatches(app, EditorPlayState::Stopped);
    }

    TEST_METHOD(PlayState_Step_UnpausesForOneFrame) {
      TestApp app;
      app.pressButtonAndProcessInput("Play");
      _assertPlayStateMatches(app, EditorPlayState::Playing);

      app.pressKeysAndProcessInput({ Key::F6 });
      _assertPlayStateMatches(app, EditorPlayState::Paused);
      app.pressKeysAndProcessInput({ Key::Shift, Key::F6 });
      _assertPlayStateMatches(app, EditorPlayState::Stepping);
      app.update();
      _assertPlayStateMatches(app, EditorPlayState::Paused);
    }

    TEST_METHOD(SingleObject_EnterPlayState_ObjectExists) {
      TestApp app;
      //Create an object with a unique name
      app.createNewObjectWithName("obj");

      //Enter play state
      app.pressKeysAndProcessInput({ Key::F5 });

      _assertPlayStateMatches(app, EditorPlayState::Playing);
      for(auto it = app.mRegistry.begin<NameTagComponent>(); it != app.mRegistry.end<NameTagComponent>(); ++it) {
        if(it->mName == "obj") {
          return;
        }
      }
      Assert::Fail(L"Created object should be found in play state");
    }
  };
}