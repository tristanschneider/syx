#pragma once

#include "ecs/ECS.h"
//TODO: annoying that these editor specifics are here, should be moved to their own file/project
#include "ecs/component/EditorComponents.h"
#include "ecs/system/editor/ObjectInspectorTraits.h"
#include "TypeInfo.h"

struct GraphicsModelComponent {
  struct Vertex {
    std::array<float, 3> mPos{};
    std::array<float, 3> mNormal{};
    std::array<float, 2> mUV{};
  };

  std::vector<Vertex> mVertices;
  std::vector<uint32_t> mIndices;
};

struct ShaderComponent {
  std::string mContents;
};

struct ShaderProgramComponent {
  Engine::Entity mVertexShader;
  Engine::Entity mPixelShader;
};

struct TextureComponent {
  size_t mWidth = 0;
  size_t mHeight = 0;
  std::vector<uint8_t> mBuffer;
};

struct GraphicsModelRefComponent {
  Engine::Entity mModel;
};

struct TextureRefComponent {
  Engine::Entity mTexture;
};

struct NeedsGpuUploadComponent {
};

//Graphics system doesn't care what "default" means as it will render them all
//this is inteded for external systems that want to change the "default"
struct DefaultViewportComponent {
};

struct CameraComponent {
  //Radians
  float mFOVX = 1.396f;
  float mFOVY = 1.396f;
  //Meters
  float mNear = 0.1f;
  float mFar = 100.0f;
};

struct ViewportComponent {
  //Points at an entity with at least a OldCameraComponent and TransformComponent
  Engine::Entity mCamera;
  //[0,1] in percentage of screen size from top left
  float mMinX = 0;
  float mMinY = 0;
  float mMaxX = 1;
  float mMaxY = 1;
};

namespace ecx {
  template<>
  struct StaticTypeInfo<TextureRefComponent> : StructTypeInfo<StaticTypeInfo<TextureRefComponent>,
    AutoTypeList<&TextureRefComponent::mTexture>,
    AutoTypeList<>> {
    static inline const std::array<std::string, 1> MemberNames = { "texture" };
    static inline constexpr const char* SelfName = "TextureRef";
  };

  template<>
  struct StaticTypeInfo<GraphicsModelRefComponent> : StructTypeInfo<StaticTypeInfo<GraphicsModelRefComponent>,
    AutoTypeList<&GraphicsModelRefComponent::mModel>,
    AutoTypeList<>> {
    static inline const std::array<std::string, 1> MemberNames = { "model" };
    static inline constexpr const char* SelfName = "GraphicsModelRef";
  };
}

template<>
struct ObjectInspectorTraits<TextureRefComponent, Engine::Entity> :
  ModalInspectorImpl<AssetInspectorModal<TextureComponent>> {};

template<>
struct ObjectInspectorTraits<GraphicsModelRefComponent, Engine::Entity> :
  ModalInspectorImpl<AssetInspectorModal<GraphicsModelComponent>> {};