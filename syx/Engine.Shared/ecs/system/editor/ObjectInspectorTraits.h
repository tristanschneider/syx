#pragma once

#include "editor/InspectorFactory.h"
#include "file/FilePath.h"

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

template<>
struct DefaultInspectorImpl<FilePath> {
  static void inspect(const char* name, FilePath& value) {
    //TODO: obey path limit and ideally edit in place
    std::string temp = value.cstr();
    Inspector::inspectString(name, temp);
    value = FilePath(temp.c_str());
  }
};

template<class ModalT>
struct ModalInspectorImpl {
  using ModalTy = ModalT;
};

template<class T, class Enabled = void> struct IsModalInspectorT : std::false_type {};
template<class T> struct IsModalInspectorT<T, std::enable_if_t<!std::is_same_v<void, typename T::ModalTy>>> : std::true_type {};

template<class TestT, template<class...> class ModalT, class Enabled = void> struct IsTemplateOfType : std::false_type {};
template<template<class...> class ModalT, class... Args> 
struct IsTemplateOfType<
  ModalT<Args...>, 
  ModalT, 
  std::enable_if_t<std::is_same_v<ModalT<Args...>, ModalT<Args...>>>
  > : std::true_type {};

//Specialize to deviate from default behavior
//TODO: what to do if multiple properties should specialize differently but have the same type?
template<class ComponentT, class PropT, class Enabled = void>
struct ObjectInspectorTraits : DefaultInspectorImpl<PropT> {
};
