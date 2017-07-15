#pragma once

//Syx stuff
#include "SyxMath.h"
#include "SyxGeometricQueries.h"
#include "SyxProfiler.h"
#include "SyxAlignmentAllocator.h"
#include "SyxAssert.h"
#include "SyxHandles.h"
#include "SyxInterface.h"
#include "SyxDebugHelpers.h"
#include "SyxDebugDrawer.h"
#include "SyxIntrusive.h"
#include "SyxStaticIndexable.h"

#include <chrono>
#ifdef SENABLED
#include <xmmintrin.h>
#endif
#include <cmath>
#include <vector>
#include <list>
#include <stack>
#include <queue>
#include <algorithm>
#include <utility>
#include <limits>
#include <string>
#include <iostream>
#include <functional>//Probably only used in tests
#include <sstream>
#include <bitset>
#include <unordered_map>
#include <unordered_set>

namespace Syx
{
  extern Syx::SyxOptions gOptions;
}