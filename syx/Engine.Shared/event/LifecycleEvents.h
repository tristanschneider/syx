#pragma once
#include "event/Event.h"

class AllSystemsInitialized : public Event {
public:
  AllSystemsInitialized();
};

class UriActivated : public Event {
public:
  UriActivated(std::string_view uri);
  std::unordered_map<std::string, std::string> mParams;
};