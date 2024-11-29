#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct VertexAttributesImpl {
  template<class C, class M>
  static const M& unwrap(const C& c, M(C::* ptr)) {
    return c.*ptr;
  }

  template<class C, class M>
  constexpr static size_t getMemberSize(M(C::*)) {
    return sizeof(M);
  }

  template<class C, class M>
  constexpr static C getClassInstance(M(C::*)) {
    return {};
  }

  static void* offset(void* v, size_t amount) {
    return static_cast<uint8_t*>(v) + amount;
  }

  static void bind(size_t index, const glm::vec2& v, size_t stride, void*& inoutOffset) {
    glEnableVertexAttribArray((GLuint)index);
    glVertexAttribPointer((GLuint)index, 2, GL_FLOAT, GL_FALSE, (GLsizei)stride, inoutOffset);
    inoutOffset = offset(inoutOffset, sizeof(v));
  }

  static void bind(size_t index, const glm::vec3& v, size_t stride, void*& inoutOffset) {
    glEnableVertexAttribArray((GLuint)index);
    glVertexAttribPointer((GLuint)index, 3, GL_FLOAT, GL_FALSE, (GLsizei)stride, inoutOffset);
    inoutOffset = offset(inoutOffset, sizeof(v));
  }

  static void bind(size_t index, float v, size_t stride, void*& inoutOffset) {
    glEnableVertexAttribArray((GLuint)index);
    glVertexAttribPointer((GLuint)index, 1, GL_FLOAT, GL_FALSE, (GLsizei)stride, inoutOffset);
    inoutOffset = offset(inoutOffset, sizeof(v));
  }

  static void unbindRange(size_t begin, size_t end) {
    for(size_t i = begin; i < end; ++i) {
      glDisableVertexAttribArray((GLuint)i);
    }
  }
};

//Provide a list of member pointers to a struct fitting the intended data layout of the vertex attributes to be sent to the gpu
template<auto... Attributes>
struct VertexAttributes {
  struct Impl {
    static constexpr size_t STRIDE = (VertexAttributesImpl::getMemberSize(Attributes) + ...);
    //Instance used to unwrap member pointers to common type so that bind functions don't need to be instantiated
    //for every unique attribute type
    static constexpr auto INSTANCE = (VertexAttributesImpl::getClassInstance(Attributes), ...);
    static_assert(sizeof(INSTANCE) == STRIDE, "Attributes should specify padding");

    template<class Att, size_t... I>
    static void bindRange(const Att& attributes, size_t begin, size_t end, std::index_sequence<I...>) {
      void* offset = nullptr;
      auto bindIf = [&](size_t index, const auto& a) {
        if(index >= begin && index < end) {
          VertexAttributesImpl::bind(index, a, STRIDE, offset);
        }
      };
      (bindIf(I, VertexAttributesImpl::unwrap(attributes, Attributes)), ...);
    }
  };

  static void bindRange(size_t begin, size_t end) {
    Impl::bindRange(Impl::INSTANCE, begin, end, std::make_index_sequence<sizeof...(Attributes)>());
  }

  static void bind() {
    bindRange(0, sizeof...(Attributes) + 1);
  }

  static void unbindRange(size_t begin, size_t end) {
    VertexAttributesImpl::unbindRange(begin, end);
  }

  static void unbind() {
    unbindRange(0, sizeof...(Attributes) + 1);
  }
};
