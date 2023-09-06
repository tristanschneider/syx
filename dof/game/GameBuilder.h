#pragma once

class IAppBuilder;
struct IDatabase;

namespace GameBuilder {
  std::unique_ptr<IAppBuilder> create(IDatabase& db);
}