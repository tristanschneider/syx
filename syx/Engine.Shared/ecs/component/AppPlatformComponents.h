#pragma once

#include "file/FilePath.h"

//Components to put on message entities for the platform to handle in its own systems
//The shared engine adds these and knows about them while only the platform project knows about the systems that handle them

struct SetWorkingDirectoryComponent {
  FilePath mDirectory;
};