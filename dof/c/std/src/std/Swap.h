#pragma once

#include <stdint.h>

#define STD_SWAP_T(T, a, b) { T swapTemp = a; a = b; b = swapTemp; }
#define STD_SWAP_I32(a, b) STD_SWAP_T(int32_t, a, b)
#define STD_SWAP_U32(a, b) STD_SWAP_T(uint32_t, a, b)