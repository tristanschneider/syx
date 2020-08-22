#pragma once
#include <shared_mutex>
class Component;

struct IComponentRegistry;

class ComponentRegistryProvider {
public:
  virtual ~ComponentRegistryProvider() = default;
  virtual std::pair<const IComponentRegistry&, std::shared_lock<std::shared_mutex>> getReader() const = 0;
  virtual std::pair<IComponentRegistry&, std::unique_lock<std::shared_mutex>> getWriter() = 0;
};