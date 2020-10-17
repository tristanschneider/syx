#pragma once
#include "event/Event.h"
#include "file/FilePath.h"
#include "Handle.h"

class ClearSpaceEvent : public TypedEvent<ClearSpaceEvent> {
public:
  ClearSpaceEvent(Handle space);
  Handle mSpace;
};

//Response is a bool indicating success
class LoadSpaceEvent : public RequestEvent<LoadSpaceEvent, bool> {
public:
  LoadSpaceEvent(Handle spaceId, const FilePath& path);
  Handle mSpace;
  FilePath mFile;
};

class SaveSpaceEvent : public TypedEvent<SaveSpaceEvent> {
public:
  SaveSpaceEvent(Handle spaceId, const FilePath& path);
  Handle mSpace;
  FilePath mFile;
};

class SetTimescaleEvent : public TypedEvent<SetTimescaleEvent> {
public:
  SetTimescaleEvent(Handle space, float timescale);
  Handle mSpace;
  float mTimescale;
};