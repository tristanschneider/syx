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

namespace ecx {
  template<>
  struct StaticTypeInfo<TextureRefComponent> : StructTypeInfo<StaticTypeInfo<TextureRefComponent>,
    AutoTypeList<&TextureRefComponent::mTexture>,
    AutoTypeList<>> {
    static inline const std::array<std::string, 1> MemberNames = { "texture" };
    static inline const std::string SelfName = "TextureRef";
  };

  template<>
  struct StaticTypeInfo<GraphicsModelRefComponent> : StructTypeInfo<StaticTypeInfo<GraphicsModelRefComponent>,
    AutoTypeList<&GraphicsModelRefComponent::mModel>,
    AutoTypeList<>> {
    static inline const std::array<std::string, 1> MemberNames = { "model" };
    static inline const std::string SelfName = "GraphicsModelRef";
  };
}

template<>
struct ObjectInspectorTraits<TextureRefComponent, Engine::Entity> :
  ModalInspectorImpl<AssetInspectorModal<TextureComponent>> {};

template<>
struct ObjectInspectorTraits<GraphicsModelRefComponent, Engine::Entity> :
  ModalInspectorImpl<AssetInspectorModal<GraphicsModelComponent>> {};