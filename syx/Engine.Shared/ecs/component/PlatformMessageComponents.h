#pragma once

#include "file/FilePath.h"
#include "SyxVec2.h"
#include "TypeList.h"
#include <variant>

struct OnFocusGainedMessageComponent {};
struct OnFocusLostMessageComponent {};
struct OnFilesDroppedMessageComponent {
  std::vector<FilePath> mFiles;
};
struct OnWindowResizeMessageComponent {
  Syx::Vec2 mNewSize;
};

using AllPlatformMessages = ecx::TypeList<
  OnFocusGainedMessageComponent,
  OnFocusLostMessageComponent,
  OnFilesDroppedMessageComponent,
  OnWindowResizeMessageComponent
>;

using AllPlatformMessagesVariant = decltype(ecx::changeType<std::variant>(AllPlatformMessages{}));