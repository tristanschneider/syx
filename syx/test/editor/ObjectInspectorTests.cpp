#include "Precompile.h"

#include "App.h"
#include "component/CameraComponent.h"
#include "component/LuaComponent.h"
#include "component/Physics.h"
#include "component/Renderable.h"
#include "CppUnitTest.h"
#include "ecs/component/TransformComponent.h"
#include "editor/MockEditorApp.h"
#include "editor/ObjectInspector.h"
#include "LuaGameObject.h"
#include "SyxQuat.h"
#include "test/TestGUIHook.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EditorTests {
  TEST_CLASS(ObjectInspectorTests) {
    static void getOrAssertObjectInspector(TestApp& app, const std::function<void(const ITestGuiQueryContext&)>& callback) {
      auto hook = Create::createAndRegisterTestGuiHook();
      //One update is needed to populate the scene tree in the gui hook
      app.update();
      MockEditorApp::findOrAssert(*MockEditorApp::getOrAssertQueryContext(*hook), ObjectInspector::WINDOW_NAME, callback, L"Inspector window should exist");
    }

    static void getOrAssertComponentList(TestApp& app, const std::function<void(const ITestGuiQueryContext&)>& callback) {
      getOrAssertObjectInspector(app, [&callback](const ITestGuiQueryContext& window) {
        MockEditorApp::findContainsOrAssert(window, ObjectInspector::COMPONENT_LIST, [&callback](const ITestGuiQueryContext& components) {
          callback(components);
        }, L"Components list should exist since object is selected");
      });
    }

    TEST_METHOD(EmptyScene_QueryInspectorWindow_IsEmpty) {
      TestApp app;

      getOrAssertObjectInspector(app, [](const ITestGuiQueryContext& window) {
        window.visitChildrenShallow([](auto&&) -> bool {
          Assert::Fail(L"Window should be empty if nothing is selected", LINE_INFO());
        });
      });
    }

    TEST_METHOD(NewObject_QueryInspectorWindow_InspectsName) {
      TestApp app;
      app.setAndUpdateSelection({ app.createNewObjectWithName("testname") });

      getOrAssertComponentList(app, [](const ITestGuiQueryContext& components) {
        MockEditorApp::findOrAssert(components, "NameTag", nullptr, L"Name category should be found");

        MockEditorApp::findOrAssert(components, "name", [](const ITestGuiQueryContext& input) {
          const auto nameInput = input.getData().tryGet<TestGuiElementData::InputText>();
          Assert::IsNotNull(nameInput, L"Name input should be an InputText", LINE_INFO());
          Assert::AreEqual(nameInput->mEditText, std::string("testname"), L"Name should match", LINE_INFO());
        }, L"Name input should be found");
      });
    }

    TEST_METHOD(NewObject_QueryInspectorWindow_InspectsTransform) {
      TestApp app;
      Engine::Entity entity = app.createNewObject();
      app.setAndUpdateSelection({ entity });
      TransformComponent& t = app.mRegistry.addComponent<TransformComponent>(entity);
      const Syx::Vec3 scale(1, 2, 3);
      const Syx::Quat rotate(Syx::Quat::axisAngle(Syx::Vec3(1, 0, 0), 0.5));
      const Syx::Vec3 translate(2, 3, 4);
      t.mValue = Syx::Mat4::transform(scale, rotate, translate);

      getOrAssertComponentList(app, [&](const ITestGuiQueryContext& components) {
        MockEditorApp::findOrAssert(components, "Transform", nullptr, L"Transform category label should exist");

        MockEditorApp::findOrAssert(components, "Translate", [&](const ITestGuiQueryContext& translateQ) {
          if(const auto data = std::get_if<TestGuiElementData::InputFloats>(&translateQ.getData().mVariant)) {
            Assert::AreEqual(data->mValues.size(), size_t(3), L"Translate should have 3 elements", LINE_INFO());
            Assert::IsTrue(translate.equal(Syx::Vec3(data->mValues[0], data->mValues[1], data->mValues[2]), 0.01f), L"Translate should match", LINE_INFO());
          }
          else {
            Assert::Fail(L"Translate should be an input float field");
          }
        }, L"Translate should be inspected");

        MockEditorApp::findOrAssert(components, "Rotate", [&](const ITestGuiQueryContext& translate) {
          if(const auto data = std::get_if<TestGuiElementData::InputFloats>(&translate.getData().mVariant)) {
            Assert::AreEqual(data->mValues.size(), size_t(4), L"Rotate should have 4 elements", LINE_INFO());
            Assert::IsTrue(rotate.mV.equal(Syx::Vec3(data->mValues[0], data->mValues[1], data->mValues[2], data->mValues[3]), 0.01f), L"Rotate should match", LINE_INFO());
          }
          else {
            Assert::Fail(L"Rotate should be an input float field");
          }
        }, L"Rotate should be inspected");

        MockEditorApp::findOrAssert(components, "Scale", [&](const ITestGuiQueryContext& translate) {
          if(const auto data = std::get_if<TestGuiElementData::InputFloats>(&translate.getData().mVariant)) {
            Assert::AreEqual(data->mValues.size(), size_t(3), L"Scale should have 3 elements", LINE_INFO());
            Assert::IsTrue(scale.equal(Syx::Vec3(data->mValues[0], data->mValues[1], data->mValues[2]), 0.01f), L"Translate should match", LINE_INFO());
          }
          else {
            Assert::Fail(L"Scale should be an input float field");
          }
        }, L"Scale should be inspected");
      });
    }

    TEST_METHOD(NewObject_AddTransformFromPicker_IsAdded) {
      TestApp app;
      Engine::Entity entity = app.createNewObject();
      app.setAndUpdateSelection({ entity });

      app.addComponentFromUI("Transform");

      Assert::IsTrue(app.mRegistry.hasComponent<TransformComponent>(entity));
    }

    /*
    TEST_METHOD(NewObject_QueryInspectorWindow_InspectsSpace) {
      MockEditorApp app;
      const LuaGameObject& obj = app.createNewObject();
      Assert::IsNotNull(obj.getComponent<SpaceComponent>(), L"Space component should be built in", LINE_INFO());

      getOrAssertComponentList(app, [&obj](const ITestGuiQueryContext& components) {
        const auto& space = *obj.getComponent<SpaceComponent>();
        MockEditorApp::findOrAssert(components, space.getTypeInfo().mTypeName, nullptr, L"Space label should be found");

        MockEditorApp::findOrAssert(components, "id", [&space](const ITestGuiQueryContext& input) {
          const auto* spaceInput = input.getData().tryGet<TestGuiElementData::InputInts>();
          Assert::IsNotNull(spaceInput, L"Space should be an int input", LINE_INFO());

          Assert::AreEqual(spaceInput->mValues.size(), size_t(1), L"Space input should only have one element", LINE_INFO());
          Assert::AreEqual(spaceInput->mValues[0], static_cast<int>(space.get()), L"Space should match", LINE_INFO());
        }, L"Space input should be found");
      });
    }

    TEST_METHOD(NewObject_AddCameraFromPicker_CameraIsAdded) {
      MockEditorApp app;

      app.createComponent(app.createNewObject(), CameraComponent::singleton().getTypeInfo());
    }

    TEST_METHOD(NewObject_AddPhysicsFromPicker_PhysicsIsAdded) {
      MockEditorApp app;

      app.createComponent(app.createNewObject(), Physics::singleton().getTypeInfo());
    }

    TEST_METHOD(NewObject_AddScriptFromPicker_ScriptIsAdded) {
      MockEditorApp app;

      app.createComponent(app.createNewObject(), LuaComponent(0).getTypeInfo());
    }

    TEST_METHOD(NewObject_AddRenderableFromPicker_RenderableIsAdded) {
      MockEditorApp app;

      app.createComponent(app.createNewObject(), Renderable::singleton().getTypeInfo());
    }
    */
  };
}