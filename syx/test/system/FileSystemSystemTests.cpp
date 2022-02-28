#include "Precompile.h"
#include "CppUnitTest.h"

#include "ecs/component/FileSystemComponent.h"
#include "ecs/system/FileSystemSystem.h"
#include "SystemRegistry.h"
#include "test/TestFileSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SystemTests {
  using namespace Engine;
  TEST_CLASS(FileSystemSystemTests) {
    struct Registry {
      Registry() {
        mSystems.registerSystem(FileSystemSystem::fileReader());
        mSystems.registerSystem(FileSystemSystem::fileWriter());
        auto fs = std::make_unique<FileSystem::TestFileSystem>();
        mFS = fs.get();
        FileSystemSystem::addFileSystemComponent(std::move(fs))->tick(mRegistry);
      }

      void tick() {
        mSystems.tick(mRegistry);
      }

      EntityRegistry* operator->() {
        return &mRegistry;
      }

      ecx::SystemRegistry<Entity> mSystems;
      EntityRegistry mRegistry;
      FileSystem::TestFileSystem* mFS = nullptr;
    };

    TEST_METHOD(NoFile_FileReadRequest_ReadFails) {
      Registry reg;
      Entity message = reg->createEntity();
      reg->addComponent<FileReadRequest>(message, FilePath("test"));

      reg.tick();

      Assert::IsTrue(reg->hasComponent<FileReadRequest>(message));
      Assert::IsTrue(reg->hasComponent<FileReadFailureResponse>(message));
    }

    TEST_METHOD(File_FileReadRequest_ReadSucceeds) {
      Registry reg;
      Entity message = reg->createEntity();
      reg->addComponent<FileReadRequest>(message, FilePath("test"));
      reg.mFS->mFiles["test"] = "contents";

      reg.tick();

      Assert::IsTrue(reg->hasComponent<FileReadRequest>(message));
      auto result = reg->tryGetComponent<FileReadSuccessResponse>(message);
      Assert::IsNotNull(result);
      Assert::IsTrue(std::string_view("contents\0", 9) == std::string_view(reinterpret_cast<const char*>(result->mBuffer.data()), result->mBuffer.size()));
    }

    TEST_METHOD(BrokenFileWrite_FileWriteRequest_WriteFails) {
      Registry reg;
      Entity message = reg->createEntity();
      reg->addComponent<FileWriteRequest>(message, FilePath("test"), std::vector<uint8_t>{ 0, 1, 2 });
      reg.mFS->mResultOverride = FileSystem::FileResult::IOError;

      reg.tick();

      Assert::IsTrue(reg->hasComponent<FileWriteRequest>(message));
      Assert::IsTrue(reg->hasComponent<FileWriteFailureResponse>(message));
    }

    TEST_METHOD(File_FileWriteRequest_FileWritten) {
      Registry reg;
      Entity message = reg->createEntity();
      reg->addComponent<FileWriteRequest>(message, FilePath("test"), std::vector<uint8_t>{ 0, 1, 2 });

      reg.tick();

      Assert::IsTrue(reg->hasComponent<FileWriteRequest>(message));
      Assert::IsTrue(reg->hasComponent<FileWriteSuccessResponse>(message));
      auto it = reg.mFS->mFiles.find("test");
      Assert::IsTrue(it != reg.mFS->mFiles.end());
      Assert::AreEqual(size_t(3), it->second.size());
      Assert::AreEqual(char(0), it->second[0]);
      Assert::AreEqual(char(1), it->second[1]);
      Assert::AreEqual(char(2), it->second[2]);
    }

    TEST_METHOD(File_RoundTrip_MatchesOriginal) {
      Registry reg;
      Entity message = reg->createEntity();
      std::vector<uint8_t> original{ 0, 1, 2 };
      reg->addComponent<FileWriteRequest>(message, FilePath("test"), original);

      reg.tick();
      reg->addComponent<FileReadRequest>(message, FilePath("test"));
      reg.tick();

      auto result = reg->tryGetComponent<FileReadSuccessResponse>(message);
      Assert::IsNotNull(result);
      //Test file system adds an extra null terminator
      original.push_back(uint8_t(0));
      Assert::IsTrue(original == result->mBuffer);
    }
  };
}