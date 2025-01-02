#pragma once

class IAppBuilder;
struct IDatabase;
struct AppEnvironment;

namespace GameBuilder {
  std::unique_ptr<IAppBuilder> create(IDatabase& db, AppEnvironment env);
}