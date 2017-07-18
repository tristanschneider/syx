#pragma once

namespace Syx
{
  class PhysicsSystem;

  struct Material
  {
    friend class PhysicsSystem;
    DeclareHandleMapNode(Material);

    Material(float density, float restitution, float friction, Handle handle):
      m_density(density), m_restitution(restitution), m_friction(friction), m_handle(handle) {}

    //Arbitrary default values that probably don't matter because they'll probably be set after the constructor if this is used
    Material(Handle handle): m_handle(handle), m_density(1.0f), m_restitution(0.0f), m_friction(0.9f) {}
    Material(void): m_handle(SyxInvalidHandle), m_density(1.0f), m_restitution(0.0f), m_friction(0.9f) {}

    bool operator==(Handle handle) { return m_handle == handle; }
    bool operator<(Handle handle) { return m_handle < handle; }

    float m_density;
    float m_restitution;
    float m_friction;
    Handle m_handle;
  };
} 