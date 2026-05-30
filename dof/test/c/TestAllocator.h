#pragma once

#include <memory>

struct std_Allocator;
typedef struct std_Allocator std_Allocator;

class TestAllocator {
public:
  TestAllocator();
  ~TestAllocator();

  std_Allocator* get();

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};