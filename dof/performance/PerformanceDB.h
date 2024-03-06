#pragma once

struct IDatabase;

namespace PerformanceDB {
  std::unique_ptr<IDatabase> create();
}