#pragma once

namespace Picker {
  struct PickerInfo {
    using ForEachCallback = std::function<void(const char* itemName, size_t itemId, const void* item)>;
    const char* name;
    //Key to store selected item in
    const char* padKey;
    std::function<void()> onCancel;
    std::function<void(const void*)> onItemPreviewed;
    std::function<void(const void*)> onItemSelected;
    //A function that takes a for each callback to get each item in the picker
    std::function<void(const ForEachCallback&)> forEachItem;
  };

  void createModal(const PickerInfo& info);
}