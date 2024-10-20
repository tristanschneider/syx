#pragma once

#include "Table.h"

namespace Loader {
  enum class LoadStep : uint8_t {
    Requested,
    Loading,
    Succeeded,
    Failed,
  };

  struct LoadState {
    LoadStep step{};
  };

  //TODO: Expose this publicly or hide tables and expose a reader interface?
  //Any asset regardleses of type will have this in its table to indicate if it has loaded
  struct LoadStateRow : Row<LoadState> {};
}