#pragma once

#include <memory>
#include <string_view>

typedef unsigned int ImGuiID;

//Extensions added on top of imgui
namespace ImGuiExt {
  enum class HookBoolResult {
    //Continue, accepting whatever the default logic computes
    Continue,
    //Regardless of what imgui logic computes, force the result to true
    ForceTrue,
    //Regardless of what imgui logic computes, force the result to false
    ForceFalse,
  };

  enum class ButtonResult {
    //Do nothing special and fall back to default button handling
    Continue,
    //Regardless of input, prevent the button from being pressed
    PreventPress,
    //Simulate a button press
    PerformPress,
  };

  //Hook in to key imgui events to allow simulating input events from test without requiring unnecessarily complicated bounds computations to try and feed mouse events
  //Implement this to customize behavior for your needs
  class TestHook {
  public:
    virtual ~TestHook() = default;

    virtual ButtonResult onButtonUpdate(ImGuiID id) = 0;
    virtual HookBoolResult shouldClip(ImGuiID id) = 0;

  protected:
    virtual bool doesIDMatch(ImGuiID id, std::string_view label) const;
  };

  void registerTestHook(std::weak_ptr<TestHook> hook);

  //Used within imgui to check for hook override behavior. Used to prevent imgui from having to worry about the lifetime or implementation details of TestHook
  namespace Hook {
    ButtonResult onButtonUpdate(ImGuiID id);
    HookBoolResult shouldClip(ImGuiID id);
  }
}