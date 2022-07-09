#include "Precompile.h"
#include "CppUnitTest.h"

#include "ecs/component/EditorComponents.h"
#include "ecs/component/GameobjectComponent.h"
#include "ecs/component/ImGuiContextComponent.h"
#include "ecs/component/RawInputComponent.h"
#include "ecs/component/SpaceComponents.h"
#include "ecs/system/editor/EditorSystem.h"
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
      //Takes two ticks, one to enqueue the command, another to process the global command queue
      app.update();
      app.update();

      Assert::AreEqual(size_t(0), app.mRegistry.size<ImGuiContextComponent>(), L"Imgui context should be removed");
    }

    TEST_METHOD(PlayState_ShiftF5_ToolboxExists) {
      TestApp app;
      app.pressButtonAndProcessInput("Play");
      _assertPlayStateMatches(app, EditorPlayState::Playing);

      app.pressKeysAndProcessInput({ Key::F5, Key::Shift });
      auto gui = Create::createAndRegisterTestGuiHook();
      app.update();
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
      app.pressButtonAndProcessInput(EditorSystem::NEW_OBJECT_LABEL);
      auto name = app.mRegistry.begin<NameTagComponent>();
      auto nameEntity = name.entity();
      Assert::IsTrue(name != app.mRegistry.end<NameTagComponent>());
      name->mName = "obj";
      auto space = app.mRegistry.find<InSpaceComponent>(name.entity());
      Assert::IsTrue(space != app.mRegistry.end<InSpaceComponent>());
      const Engine::Entity editorSpace = space.component().mSpace;

      //Enter play state
      app.pressKeysAndProcessInput({ Key::F5 });
      //Give several ticks to load
      for(int i = 0; i < 15; ++i) {
        app.update();
      }

      _assertPlayStateMatches(app, EditorPlayState::Playing);
      Assert::AreEqual(size_t(1), app.mRegistry.size<NameTagComponent>());
      for(auto it = app.mRegistry.begin<NameTagComponent>(); it != app.mRegistry.end<NameTagComponent>(); ++it) {
        if(it->mName == "obj") {
          space = app.mRegistry.find<InSpaceComponent>(it.entity());
          Assert::IsTrue(it.entity() != nameEntity, L"Entity should be different than original because it's in the play space than the editor space");
          Assert::IsTrue(space != app.mRegistry.end<InSpaceComponent>());
          const Engine::Entity inSpace = space.component().mSpace;
          Assert::IsTrue(app.mRegistry.isValid(inSpace));
          Assert::IsTrue(app.mRegistry.find<DefaultPlaySpaceComponent>(space.component().mSpace) != app.mRegistry.end<DefaultPlaySpaceComponent>(), L"Entity should be in play space");
          return;
        }
      }
      Assert::Fail(L"Created object should be found in play state");
    }

    TEST_METHOD(SingleObject_ExitPlayState_ObjectReverted) {
      TestApp app;
      //Create an object with a unique name
      app.pressButtonAndProcessInput(EditorSystem::NEW_OBJECT_LABEL);
      auto name = app.mRegistry.begin<NameTagComponent>();
      Assert::IsTrue(name != app.mRegistry.end<NameTagComponent>());
      name->mName = "obj";

      //Enter play state
      app.pressKeysAndProcessInput({ Key::F5 });
      //Give several ticks to load
      for(int i = 0; i < 15; ++i) {
        app.update();
      }

      Assert::AreEqual(size_t(1), app.mRegistry.size<NameTagComponent>());
      auto it = app.mRegistry.begin<NameTagComponent>();
      //Change the name so the revert can be observed when exiting the play state
      it->mName = "something else";

      _assertPlayStateMatches(app, EditorPlayState::Playing);

      //Exit play state
      app.pressKeysAndProcessInput({ Key::Shift, Key::F5 });

      for(int i = 0; i < 15; ++i) {
        app.update();
      }

      Assert::AreEqual(size_t(1), app.mRegistry.size<NameTagComponent>());
      it = app.mRegistry.begin<NameTagComponent>();
      Assert::AreEqual(std::string("obj"), it->mName, L"Change in play state should have been reverted");
    }
  };
}