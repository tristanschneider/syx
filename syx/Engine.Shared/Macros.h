#pragma once

#define DEBUG_ASSERT(condition, msg) assert(condition && msg)
#define DEBUG_LOG(msg, ...) printf(msg, __VA_ARGS__)