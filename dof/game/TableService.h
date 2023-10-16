#pragma once

class IAppBuilder;

//This is a service responsible for processing deferred requests to do table operations
//This currently means:
//- Process Events::onRemovedElement events to remove those elements
namespace TableService {
  void processEvents(IAppBuilder& builder);
}