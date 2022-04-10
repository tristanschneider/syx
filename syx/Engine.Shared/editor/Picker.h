#pragma once

struct PickerContextComponent;

namespace Picker {
  struct PickerInfo {
    using ForEachCallback = std::function<void(const char* itemName, size_t itemId, const void* item)>;
    const char* name = nullptr;
    //Key to store selected item in
    const char* padKey = nullptr;
    std::function<void()> onCancel;
    std::function<void(const void*)> onItemPreviewed;
    std::function<void(const void*)> onItemSelected;
    //A function that takes a for each callback to get each item in the picker
    std::function<void(const ForEachCallback&)> forEachItem;
  };

  enum class ModalResult {
    Cancel,
    Continue,
  };

  enum class PickItemResult {
    ItemPreviewed,
    ItemSelected,
    Continue,
  };

  struct ImmediatePickerInfo {
    using ForEachCallback = std::function<PickItemResult(const char* itemName, size_t itemId)>;
    const char* name = nullptr;
    //A function that takes a for each callback to get each item in the picker
    std::function<void(const ForEachCallback&)> forEachItem;
  };

  void createModal(const PickerInfo& info);
  ModalResult createModal(const ImmediatePickerInfo& info, PickerContextComponent& context);
}