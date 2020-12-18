#pragma once
#include "targetver.h"
#include "CppUnitTest.h"

#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <algorithm>
#include <stdio.h>
#include <string>
#include <algorithm>
#include <chrono>
#include <thread>
#include <fstream>
#include <memory>
#include <mutex>
#include <type_traits>
#include <condition_variable>
#include <thread>
#include <functional>
#include <atomic>
#include <cstdio>
#include <cassert>
#include <cstdint>

#include "util/TypeId.h"

namespace Test {
  // Hack to get LINE_INFO not to trigger error c2102
  template<typename T>
  T *lineInfoHack(T &&v) {
    return &v;
  }
}

#undef LINE_INFO
#define  LINE_INFO() Test::lineInfoHack(Microsoft::VisualStudio::CppUnitTestFramework::__LineInfo(__WFILE__, __FUNCTION__, __LINE__))