#pragma once
#include "event/Event.h"
#include "file/FilePath.h"
#include "Handle.h"

class ClearSpaceEvent : public Event {
public:
  ClearSpaceEvent(Handle space);
  Handle mSpace;
};

class LoadSpaceEvent : public Event {
public:
  LoadSpaceEvent(Handle spaceId, const FilePath& path);
  Handle mSpace;
  FilePath mFile;
};

class SaveSpaceEvent : public Event {
public:
  SaveSpaceEvent(Handle spaceId, const FilePath& path);
  Handle mSpace;
  FilePath mFile;
};

class SetTimescaleEvent : public Event {
public:
  SetTimescaleEvent(Handle space, float timescale);
  Handle mSpace;
  float mTimescale;
};