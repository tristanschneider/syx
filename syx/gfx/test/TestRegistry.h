#pragma once

class TestRegistry {
public:
  using TestFunc = void(*)();

  static TestRegistry& get() {
    static TestRegistry singleton;
    return singleton;
  }

  void run() const {
    printf("Running tests...\n");
    for(const auto& test : mTests) {
      printf("%s\n", test.second);
      test.first();
    }
    printf("Done\n");
  }

  void pushTest(TestFunc func, const char* name) {
    mTests.push_back({func, name});
  }

private:
  std::vector<std::pair<TestFunc, const char*>> mTests;
};

struct TestRegistrar {
  TestRegistrar(void(*func)(), const char* name) {
    TestRegistry::get().pushTest(func, name);
  }
};
#define TEST_FUNC(name) void name(); namespace { TestRegistrar name##_reg(&name, #name); } void name()
#define TEST_ASSERT(condition, message) assert(condition && message)
