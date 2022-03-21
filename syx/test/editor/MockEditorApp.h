#pragma once
#include "ecs/ECS.h"
#include "ecs/EngineAppContext.h"
#include "Handle.h"
#include "test/MockApp.h"

class Camera;
class Component;
struct ComponentTypeInfo;
template<class Callable>
class FinalAction;
struct ITestGuiHook;
struct ITestGuiQueryContext;
enum class Key : uint8_t;
class LuaGameObject;

namespace EditorTests {
  struct MockEditorApp : public MockApp {
    using ScopedAssertion = FinalAction<::std::function<void()>>;

    MockEditorApp();
    ~MockEditorApp();
    //Await processing of an event that is expected to be fired in the next tick. So pushing a message then calling this would be enough to expect a response message having been processed
    void processEditorInput();
    //Simulate a button press expected to trigger an event on the next update, then await the processing of that event
    void pressButtonAndProcessInput(const ::std::string& label);
    void pressKeyAndProcessInput(Key key);
    void pressKeysAndProcessInput(const ::std::initializer_list<Key>& keys);
    //Set a selection and propagate the change to the editor
    void setAndUpdateSelection(::std::vector<Handle> selection);
    ScopedAssertion createScopedNetObjectCountAssertion(int netChange, const ::std::wstring& assertMessage);
    const LuaGameObject& createNewObject();
    const LuaGameObject& findGameObjectOrAssert(const std::string_view name);
    const Component& createComponent(const LuaGameObject& object, const ComponentTypeInfo& type);
    //Arbitrary camera values that result in Camera::isValid returning true
    static Camera _createValidCamera();
    //One update to process the event and another for the response to be processed
    void _updateForEventResponse();
    ::std::vector<Handle> simulateMousePick(const ::std::vector<Handle>& objs);
    ::std::vector<Handle> simulateMousePick(Handle obj);

    static std::shared_ptr<ITestGuiQueryContext> getOrAssertQueryContext(ITestGuiHook& hook);
    //Find a shallow child node who triggers a false return of the callback
    static void invokeOrAssert(const ITestGuiQueryContext& query, const std::function<bool(const ITestGuiQueryContext&)> callback, const std::wstring& assertMsg);
    //Find a shallow child node whose name matches the given string
    static void findOrAssert(const ITestGuiQueryContext& query, const std::string& name, const std::function<void(const ITestGuiQueryContext&)> callback, const std::wstring& assertMsg);
    //Find a shallow child node whose name contains the given string
    static void findContainsOrAssert(const ITestGuiQueryContext& query, const std::string& name, const std::function<void(const ITestGuiQueryContext&)> callback, const std::wstring& assertMsg);
  };

  struct TestApp {
    TestApp();
    void pressButtonAndProcessInput(const std::string& label);
    void update();

    Engine::AppContext mContext{ std::make_shared<Engine::Scheduler>(ecx::SchedulerConfig{}) };
    Engine::EntityRegistry mRegistry;
  };
}