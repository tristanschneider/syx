#pragma once

#include "generics/IndexRange.h"
#include "loader/Reflection.h"
#include "RuntimeTable.h"

class IAppBuilder;
class IAppModule;
class RuntimeDatabase;
class RuntimeDatabaseTaskBuilder;

namespace Reflection {
  class IRowLoader {
  public:
    virtual ~IRowLoader() = default;

    virtual DBTypeID getTypeID() const = 0;
    virtual std::string_view getName() const = 0;
    virtual void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) const = 0;
  };

  template<class T>
  concept HasHash = requires() {
    typename T::src_row;
    { T::NAME } -> std::convertible_to<std::string_view>;
  };
  template<class T>
  concept HasLoadFn = requires(const IRow& s, RuntimeTable& dst, gnx::IndexRange range) {
    T::load(s, dst, range);
  };
  template<class T> concept StaticLoader = HasLoadFn<T>;
  template<class T> concept StaticNamedLoader = HasHash<T> && StaticLoader<T>;

  template<IsRow Src, Loader::IsLoadableRow Dst>
  struct DirectRowLoader {
    using src_row = Src;
    static constexpr std::string_view NAME = Dst::KEY;

    static void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) {
      const Src& s = static_cast<const Src&>(src);
      if(Dst* dstRow = dst.tryGet<Dst>()) {
        size_t srcI{};
        for(size_t i : range) {
          dstRow->at(i) = static_cast<typename Dst::ElementT>(s.at(srcI));
        }
      }
    }
  };

  template<class R, class S, class FN>
  bool tryLoadRow(const S& s, RuntimeTable& dst, gnx::IndexRange range, FN fn) {
    if(auto row = dst.tryGet<R>()) {
      //Source is transferring all elements so is 0 to max
      //Destination is shifted by any previously existing elements
      size_t srcI{};
      for(size_t i : range) {
        row->at(i) = typename R::ElementT{ fn(s.at(srcI++)) };
      }
      return true;
    }
    return false;
  }

  template<StaticLoader L>
  std::unique_ptr<IRowLoader> createRowLoader(L, std::string_view name) {
    struct Impl : IRowLoader {
      Impl(std::string_view n)
        : name{ n } {
      }

      DBTypeID getTypeID() const final { return Loader::getDynamicRowKey<typename L::src_row>(getName()); }

      std::string_view getName() const final { return name; }

      void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) const final {
        L::load(src, dst, range);
      }

      std::string_view name;
    };
    return std::make_unique<Impl>(name);
  }

  //Creates a loader from a type that looks like this:
  //struct L {
  //  // Any of the row types in Reflection.h
  //  using src_row = Loader::BoolRow;
  //  // The desired row name exposed to imported files
  //  static constexpr std::string_view NAME = "Name";
  //  // Implementation of the loading, use tryLoadRow if desired
  //  static void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) {}
  //}
  //DirectRowLoader also satisfies this
  template<StaticNamedLoader L>
  std::unique_ptr<IRowLoader> createRowLoader(L l) {
    return createRowLoader(l, L::NAME);
  }

  template<class Src, class Dst>
  std::unique_ptr<IRowLoader> createDirectRowLoader() {
    return createRowLoader(DirectRowLoader<Src, Dst>{});
  }

  class IReflectionReader {
  public:
    virtual ~IReflectionReader() = default;

    virtual void loadFromDBIntoGame(RuntimeDatabase& toRead) = 0;
  };

  //Can be called any time after createDatabase of the ReflectionModule
  void registerLoaders(IAppBuilder& builder, std::vector<std::unique_ptr<IRowLoader>>&& loaders);

  template<class... Loaders>
  void registerLoaders(IAppBuilder& builder, Loaders&&... loaders) {
    std::vector<std::unique_ptr<IRowLoader>> load;
    load.reserve(sizeof...(loaders));
    (load.push_back(std::move(loaders)), ...);
    registerLoaders(builder, std::move(load));
  }

  std::unique_ptr<IReflectionReader> createReader(RuntimeDatabaseTaskBuilder& task);
}

namespace ReflectionModule {
  //Should be registered last as its dependentInit will finalize the list of loadable tables
  std::unique_ptr<IAppModule> create();
}