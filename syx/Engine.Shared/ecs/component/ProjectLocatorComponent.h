#pragma once

#include "ProjectLocator.h"

class ProjectLocatorComponent {
public:
  ProjectLocatorComponent(std::unique_ptr<ProjectLocator> locator = nullptr)
    : mLocator(locator ? std::move(locator) : std::make_unique<ProjectLocator>()) {
  }

  ProjectLocatorComponent(ProjectLocatorComponent&&) = default;
  ProjectLocatorComponent& operator=(ProjectLocatorComponent&&) = default;

  ProjectLocator& get() {
    return *mLocator;
  }

  const ProjectLocator& get() const {
    return *mLocator;
  }

private:
  std::unique_ptr<ProjectLocator> mLocator;
};