#pragma once

#include <memory>

struct std_Allocator;
typedef struct std_Allocator std_Allocator;

class TestAllocator {
public:
  TestAllocator();
  ~TestAllocator();

  std_Allocator* get();
  operator std_Allocator*();

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};