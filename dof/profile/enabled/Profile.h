#pragma once

#include "microprofile.h"

#define PROFILE_SCOPE(area, name) MICROPROFILE_SCOPEI(area, name, 0)

#define PROFILE_UPDATE(context) MicroProfileFlip(context)

using ProfileToken = uint64_t;

#define PROFILE_CREATETOKEN(area, name, color) MicroProfileGetToken(area, name, color, MicroProfileTokenTypeCpu, 0)
#define PROFILE_ENTER_TOKEN(token) MicroProfileEnter(token)
#define PROFILE_EXIT_TOKEN(token) MicroProfileLeave()