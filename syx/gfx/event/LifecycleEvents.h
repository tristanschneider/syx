#pragma once
#include "event/Event.h"

class AllSystemsInitialized : public Event {
public:
  AllSystemsInitialized();
};

class UriActivated : public Event {
public:
  UriActivated(std::string uri);
  std::string mUri;
};