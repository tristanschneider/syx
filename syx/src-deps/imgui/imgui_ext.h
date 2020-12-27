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
    virtual void onWindowBegin(ImGuiID id, const char* name) = 0;
    virtual void onWindowEnd() = 0;
    virtual void onButtonCreated(ImGuiID id, const char* name) = 0;
    virtual void onTextCreated(const char* text) = 0;
    virtual void onInputTextCreated(const char* label, std::string_view buffer, const void* userdata) = 0;
    virtual void onCheckboxCreated(const char* label, bool value) = 0;
    virtual void onInputFloatsCreated(const char* label, const float* elements, size_t elementCount) = 0;
    virtual void onInputIntsCreated(const char* label, const int* elements, size_t elementCount) = 0;

  protected:
    virtual bool doesIDMatch(ImGuiID id, std::string_view label) const;
  };

  void registerTestHook(std::weak_ptr<TestHook> hook);

  //Used within imgui to check for hook override behavior. Used to prevent imgui from having to worry about the lifetime or implementation details of TestHook
  namespace Hook {
    ButtonResult onButtonUpdate(ImGuiID id);
    HookBoolResult shouldClip(ImGuiID id);
    void onWindowBegin(ImGuiID id, const char* name);
    void onWindowEnd();
    //Called after clipping
    void onButtonCreated(ImGuiID id, const char* name);
    //Any hook in an editable ui element hooks the value BEFORE modification
    //Text before printf style formatting
    void onTextCreated(const char* text);
    void onInputTextCreated(const char* label, std::string_view buffer, const void* userdata);
    void onCheckboxCreated(const char* label, bool value);
    void onInputFloatsCreated(const char* label, const float* elements, size_t elementCount);
    void onInputIntsCreated(const char* label, const int* elements, size_t elementCount);
  }
}