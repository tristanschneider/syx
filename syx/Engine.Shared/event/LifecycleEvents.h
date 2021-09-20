#pragma once
#include "event/Event.h"

class AllSystemsInitialized : public TypedEvent<AllSystemsInitialized> {
public:
  AllSystemsInitialized() = default;
};

class UriActivated : public TypedEvent<UriActivated> {
public:
  UriActivated(std::string_view uri);
  std::unordered_map<std::string, std::string> mParams;
};

struct FrameStart : public TypedEvent<FrameStart> {
};

struct FrameEnd : public TypedEvent<FrameEnd> {
};

struct WindowResize : public TypedEvent<WindowResize> {
  WindowResize(int width, int height)
    : mWidth(width)
    , mHeight(height) {
  }

  int mWidth = 0;
  int mHeight = 0;
};