#include "Precompile.h"
#include "config/ConfigIO.h"

#include <cereal/archives/json.hpp>
#include <cereal/types/optional.hpp>

namespace cereal {
  //This macro will archive all the input arguments while also using their member name as the key in the output, so ARCHIVE(archive, obj.member) has "member" as a key
  #define ARCHIVE(A, ...) archiveAllWithName(A, #__VA_ARGS__, __VA_ARGS__);

  struct ArchiverData {
    bool isDeserialize{};
    const Config::IFactory* factory{};
  };

  //An implementation of archiver that passes along some extra information for use during the archival
  //It does an unsafe downcast that only makes sense if all archive usages use this wrapper
  template<class Archiver>
  struct ArchiverWithData : Archiver {
    using Archiver::Archiver;

    static ArchiverData& get(Archiver& archiver) {
      return static_cast<ArchiverWithData&>(archiver).data;
    }

    ArchiverData data;
  };

  template<class Archiver, size_t... S, class... Args>
  void archiveAllWithName(Archiver& archiver, const char* names, std::index_sequence<S...>, Args&... args) {
    std::string buffer{ names };
    constexpr size_t argCount = sizeof...(args);
    if constexpr(argCount > 0) {
      //VA_ARGS is a single string of the entire argument list separated by commas, split them up by '.' since it's all "val.a, val.b, ..."
      std::array<const char*, sizeof...(args)> strings;
      int found = 0;
      char* currentChar = buffer.data();
      while(*currentChar != 0) {
        if(*currentChar == '.') {
          //Skip the dot
          strings[found++] = currentChar + 1;
          if(found >= strings.size()) {
            break;
          }
        }
        //Null terminate previously found substring
        else if(*currentChar == ',') {
          *currentChar = 0;
        }
        ++currentChar;
      }

      ((archiver.setNextName(strings[S]), archiver(args)), ...);
      archiver;names;(args, ...);
    }
  }

  template<class Archiver, class... Args>
  void archiveAllWithName(Archiver& archiver, const char* names, Args&... args) {
    archiveAllWithName(archiver, names, std::index_sequence_for<Args...>(), args...);
  }

  template<class Archive>
  void serialize(Archive& archive, Config::PlayerConfig& value) {
    ARCHIVE(archive,
      value.drawMove,
      value.linearSpeedCurve,
      value.linearForceCurve,
      value.angularSpeedCurve,
      value.angularForceCurve,
      value.linearStoppingSpeedCurve,
      value.linearStoppingForceCurve,
      value.angularStoppingSpeedCurve,
      value.angularStoppingForceCurve
    );
  }

  template<class Archive>
  void serialize(Archive& archive, Config::PlayerAbilityConfig& value) {
    ARCHIVE(archive,
      value.explodeLifetime,
      value.explodeStrength
    );
  }
  
  template<class Archive>
  void serialize(Archive& archive, Config::PhysicsConfig& value) {
    ARCHIVE(archive,
      value.mForcedTargetWidth,
      value.linearDragMultiplier,
      value.angularDragMultiplier,
      value.drawCollisionPairs,
      value.drawContacts,
      value.solveIterations,
      value.frictionCoeff
    );
  }
  
  template<class Archive>
  void serialize(Archive& archive, Config::CameraConfig& value) {
    ARCHIVE(archive,
      value.cameraZoomSpeed,
      value.followCurve,
      value.zoom
    );
  }
  
  template<class Archive>
  void serialize(Archive& archive, Config::FragmentConfig& value) {
    ARCHIVE(archive,
      value.fragmentColumns,
      value.fragmentRows,
      value.fragmentGoalDistance
    );
  }
  
  template<class Archive>
  void serialize(Archive& archive, Config::WorldConfig& value) {
    ARCHIVE(archive,
      value.deltaTime,
      value.boundarySpringConstant
    );
  }
  
  template<class Archive>
  void serialize(Archive& archive, Config::CurveConfigExt& value) {
    const ArchiverData& data = ArchiverWithData<Archive>::get(archive);
    //If this is a serialize call, take the value from the adapter and serialize it
    Config::CurveConfig v;
    if(!value.adapter && data.factory) {
      value = data.factory->createCurve();
    }
    if(!data.isDeserialize && value.adapter) {
      v = value.adapter->read();
    }

    ARCHIVE(archive,
      v.scale,
      v.offset,
      v.duration,
      v.flipInput,
      v.flipOutput,
      v.curveFunction
    );

    //If this is a deserialize call, take the value from the config and inform the adapter
    if(data.isDeserialize && value.adapter) {
      value.adapter->write(v);
    }
  }

  template<class Archive>
  void serialize(Archive& archive, Config::GameConfig& value) {
    ARCHIVE(archive,
      value.ability,
      value.camera,
      value.fragment,
      value.physics,
      value.player,
      value.world
    );
  }
}

namespace ConfigIO {
  std::string serializeJSON(const Config::GameConfig& config) {
    std::stringstream stream;
    {
      cereal::ArchiverWithData<cereal::JSONOutputArchive> archive(stream);
      archive.data.isDeserialize = false;
      archive.data.factory = nullptr;
      archive(config);
    }
    return stream.str();
  }

  Result deserializeJson(const std::string& buffer, const Config::IFactory& factory) {
    factory;
    std::stringstream stream{ std::string(buffer) };
    Result result;

    try {
      Config::GameConfig temp;
      cereal::ArchiverWithData<cereal::JSONInputArchive> archive(stream);
      archive.data.isDeserialize = true;
      archive.data.factory = &factory;
      archive(temp);
      result.value = std::move(temp);
    }
    catch(std::exception& e) {
      result.value = Result::Error{ e.what() };
    }
    return result;
  }
}
