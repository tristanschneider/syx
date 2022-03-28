#pragma once

#include "editor/InspectorFactory.h"

template<class PropT>
struct DefaultInspectorImpl {
};

//Default widgets for basic data types when unspecialized
template<>
struct DefaultInspectorImpl<std::string> {
  static void inspect(const char* name, std::string& value) {
    Inspector::inspectString(name, value);
  }
};

//Specialize to deviate from default behavior
//TODO: what to do if multiple properties should specialize differently but have the same type?
template<class ComponentT, class PropT, class Enabled = void>
struct ObjectInspectorTraits : DefaultInspectorImpl<PropT> {
};
