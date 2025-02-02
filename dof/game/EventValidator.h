#pragma once

class IAppModule;

//Asserts that events are emitted and processed consistently. Intended for debugging.
namespace EventValidator {
  std::unique_ptr<IAppModule> createModule(std::string_view name);
}