#pragma once

#include "microprofile.h"

#define PROFILE_SCOPE(area, name) MICROPROFILE_SCOPEI(area, name, 0)

#define PROFILE_UPDATE(context) MicroProfileFlip(context)