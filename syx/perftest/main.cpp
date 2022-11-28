#include <cstdio>
#include <tuple>
#include <vector>
#include <chrono>
#include <string>

#include "out_ispc/CollisionDetection.h"
#include "out_ispc/Inertia.h"
#include "out_ispc/Integrator.h"

template<auto Fn>
struct TestHarness;

//Holds what is needed to apply the argument and convert from the stored type to the parameter type
template<class T, class Enabled = void>
struct ArgumentHolder {
  void setSize(size_t) {};

  T getValue() {
    return mValue;
  }

  T mValue;
};

struct UniformSymmetricMatrixHolderImpl {
  void setSize(size_t size) {
    a.resize(size);
    b.resize(size);
    c.resize(size);
    d.resize(size);
    e.resize(size);
    f.resize(size);
  }

  ispc::UniformSymmetricMatrix& getValue() {
    mValue.a = a.data();
    mValue.b = b.data();
    mValue.c = c.data();
    mValue.d = d.data();
    mValue.e = e.data();
    mValue.f = f.data();
    return mValue;
  }

  std::vector<float> a, b, c, d, e, f;
  ispc::UniformSymmetricMatrix mValue;
};

template<class T>
struct ArgumentHolder<T, std::enable_if_t<std::is_same_v<ispc::UniformSymmetricMatrix, std::decay_t<T>>>>
  : UniformSymmetricMatrixHolderImpl {};

template<class T>
struct ArgumentHolder<T, std::enable_if_t<std::is_same_v<ispc::UniformQuat, std::decay_t<T>>>> {
  void setSize(size_t size) {
    i.resize(size);
    j.resize(size);
    k.resize(size);
    w.resize(size);
  }

  ispc::UniformQuat& getValue() {
    mValue.i = i.data();
    mValue.j = j.data();
    mValue.k = k.data();
    mValue.w = w.data();
    return mValue;
  }

  std::vector<float> i, j, k, w;
  ispc::UniformQuat mValue;
};

template<class T>
struct ArgumentHolder<T, std::enable_if_t<std::is_same_v<ispc::UniformConstQuat, std::decay_t<T>>>> {
  void setSize(size_t size) {
    i.resize(size);
    j.resize(size);
    k.resize(size);
    w.resize(size);
  }

  ispc::UniformConstQuat& getValue() {
    mValue.i = i.data();
    mValue.j = j.data();
    mValue.k = k.data();
    mValue.w = w.data();
    return mValue;
  }

  std::vector<float> i, j, k, w;
  ispc::UniformConstQuat mValue;
};


template<class T>
struct ArgumentHolder<T, std::enable_if_t<std::is_same_v<ispc::UniformVec3, std::decay_t<T>>>> {
  void setSize(size_t size) {
    x.resize(size);
    y.resize(size);
    z.resize(size);
  }

  ispc::UniformVec3& getValue() {
    mValue.x = x.data();
    mValue.y = y.data();
    mValue.z = z.data();
    return mValue;
  }

  std::vector<float> x, y, z;
  ispc::UniformVec3 mValue;
};

template<class T>
struct ArgumentHolder<T, std::enable_if_t<std::is_same_v<ispc::UniformConstVec3, std::decay_t<T>>>> {
  void setSize(size_t size) {
    x.resize(size);
    y.resize(size);
    z.resize(size);
  }

  ispc::UniformConstVec3& getValue() {
    mValue.x = x.data();
    mValue.y = y.data();
    mValue.z = z.data();
    return mValue;
  }

  std::vector<float> x, y, z;
  ispc::UniformConstVec3 mValue;
};


template<class T>
struct ArgumentHolder<T*> {
  void setSize(size_t size) {
    mValues.resize(size);
  }

  T* getValue() {
    return mValues.data();
  }

  //Strip const in stored value
  std::vector<std::decay_t<T>> mValues;
};

template<>
struct ArgumentHolder<uint32_t> {
  void setSize(size_t size) {
    mSize = static_cast<uint32_t>(size);
  }

  uint32_t getValue() {
    return mSize;
  }

  uint32_t mSize = 0;
};

template<size_t, class T>
struct IndexedT : T {};

template<class... Args>
struct ArgumentsCollection {
  void setSize(size_t size) {
    mArgs.visit([&size](auto&... holders) {
      (holders.setSize(size), ...);
    });
  }

  template<class T>
  void apply(const T& func) {
    mArgs.visit([&func](auto&... holders) {
      func(holders.getValue()...);
    });
  }

  template<class T>
  struct Impl;

  template<size_t... Indices>
  struct Impl<std::index_sequence<Indices...>> {
    template<class T>
    void visit(const T& visitor) {
      visitor(std::get<Indices>(mArgs)...);
    }

    std::tuple<IndexedT<Indices, ArgumentHolder<Args>>...> mArgs;
  };

  Impl<std::make_index_sequence<sizeof...(Args)>> mArgs;
};

template<class R, class... Args, R(*Func)(Args...)>
struct TestHarness<Func> {
  void setSize(size_t size) {
    mArgs.setSize(size);
  }

  void execute() {
    mArgs.apply(Func);
  }

  ArgumentsCollection<Args...> mArgs;
};

template<auto Fn>
uint64_t runIterations(size_t iterations, size_t valueCount) {
  uint64_t total = 0;
  for(size_t i = 0; i < iterations; ++i) {
    // While it's slower to put the allocation in the loop like this, the changing memory seems to lead to more consistent averages
    TestHarness<Fn> harness;
    harness.setSize(valueCount);

    auto before = std::chrono::steady_clock::now();

    harness.execute();

    total += (std::chrono::steady_clock::now() - before).count();
  }

  // Average
  return total / iterations;
}

void summarize(const char* name, size_t iterations, size_t valueCount, uint64_t time) {
  printf("%sns %s with %s iterations and %s values\n", std::to_string(time).c_str(), name, std::to_string(iterations).c_str(), std::to_string(valueCount).c_str());
}

template<auto Fn>
void runAndSummarize(const char* name, size_t iterations, size_t valueCount) {
  summarize(name, iterations, valueCount, runIterations<Fn>(iterations, valueCount));
}


int main() {
  const size_t ITERATIONS = 1000;
  const size_t VALUES = 10000;

  for(size_t i = 0; i < 3; ++i) {
    runAndSummarize<&ispc::integrateLinearPosition>("integrateLinearPosition", ITERATIONS, VALUES);
    runAndSummarize<&ispc::integrateLinearVelocityGlobalAcceleration>("integrateVelocity", ITERATIONS, VALUES);
    runAndSummarize<&ispc::integrateRotation>("integrateRotation", ITERATIONS, VALUES);
    runAndSummarize<&ispc::recomputeInertiaTensor>("recomputeInertiaTensor", ITERATIONS, VALUES);
    runAndSummarize<&ispc::computeSphereMass>("sphereMass", ITERATIONS, VALUES);
    runAndSummarize<&ispc::invertMass>("invertMass", ITERATIONS, VALUES);
    runAndSummarize<&ispc::computeContactSphereSphere>("sphereContact", ITERATIONS, VALUES);
    printf("\n");
  }
}