#include "Precompile.h"
#include "event/LifecycleEvents.h"

#include "Util.h"

UriActivated::UriActivated(std::string_view uri) {
  const std::vector<std::string_view> params = Util::split(uri, " ");
  for(std::string_view param : params) {
    const std::vector<std::string_view> keyValue = Util::split(param, "=");
    if(keyValue.size() == 2) {
      mParams[std::string(keyValue[0])] = std::string(keyValue[1]);
    }
    else {
      printf("Invalid argument %s\n", param.data());
    }
  }
}