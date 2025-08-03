#pragma once

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include <glm/gtx/norm.hpp>

namespace Transform {
  struct Parts {
    glm::vec2 rot{ 1, 0 };
    glm::vec2 scale{ 1, 1 };
    glm::vec3 translate{};
  };

  //2d transform matrix that omits the unused third row except for a Z position as a special case, allowing no Z scale or rotation.
  //"a" and "b" are the basis vectors in the columns, "t" is the third column which is translation
  struct PackedTransform {
    constexpr PackedTransform operator*(const PackedTransform& v) const {
      return {
        ax*v.ax + bx*v.ay, ax*v.bx + bx*v.by, ax*v.tx + bx*v.ty + tx,
        ay*v.ax + by*v.ay, ay*v.bx + by*v.by, ay*v.tx + by*v.ty + ty,
                                              v.tz + tz
      };
    }

    constexpr glm::vec3 transformPoint(const glm::vec3& v) const {
      //Matrix multiplication with [v.x, v.y, v.z, 1]
      return {
        ax*v.x + bx*v.y + tx,
        ay*v.x + by*v.y + ty,
        v.z + tz
      };
    }

    constexpr glm::vec2 transformPoint(const glm::vec2& v) const {
      //Matrix multiplication with [v.x, v.y, 0, 1]
      return {
        ax*v.x + bx*v.y + tx,
        ay*v.x + by*v.y + ty,
      };
    }

    constexpr glm::vec3 transformVector(const glm::vec3& v) const {
      //Matrix multiplication with [v.x, v.y, v.z, 0]
      return {
        ax*v.x + bx*v.y,
        ay*v.x + by*v.y,
        v.z
      };
    }

    constexpr glm::vec2 transformVector(const glm::vec2& v) const {
      //Matrix multiplication with [v.x, v.y, 0, 0]
      return {
        ax*v.x + bx*v.y,
        ay*v.x + by*v.y,
      };
    }

    Parts decompose() const {
      //These must be non-null or it wouldn't be a valid transform
      const float aLen = std::sqrt(ax*ax + ay*ay);
      const float bLen = std::sqrt(bx*bx + by*by);
      return Parts{
        .rot = glm::vec2{ ax/aLen, ay/aLen },
        .scale = glm::vec2{ aLen, bLen },
        .translate = glm::vec3{ tx, ty, tz }
      };
    }

    constexpr glm::vec2 pos2() const {
      return { tx, ty };
    }

    constexpr glm::vec3 pos3() const {
      return { tx, ty, tz };
    }

    constexpr void setPos(const glm::vec2& t) {
      tx = t.x;
      ty = t.y;
    }

    constexpr void setPos(const glm::vec3& t) {
      tx = t.x;
      ty = t.y;
      tz = t.z;
    }

    static constexpr PackedTransform build(const Parts& parts) {
      //Multiplication of these, except a and b are rot and orthogonal to rot [-y, x]
      //[1 0 tx][ax bx 0][sx  0 0] [ax*sx, -ay*sy, tx]
      //[0 1 ty][ay by 0][ 0 sy 0]=[ay*sx,  ax*sy, ty]
      //[0 0  1][ 0  0 1][ 0  0 1] [    0       0, tz]
      return {
        parts.rot.x*parts.scale.x, -parts.rot.y*parts.scale.y, parts.translate.x,
        parts.rot.y*parts.scale.x,  parts.rot.x*parts.scale.y, parts.translate.y,
                                                               parts.translate.z
      };
    }

    static PackedTransform inverse(const Parts& p) {
      //Multiplication of transform, rotate, scale, all inverse, all in reverse order
      //[1/sx 0 0][ax  ay 0][1 0 -tx] [ax/sx, ay/sx, -ax*tx/sx - ay*ty/sx]
      //[0 1/sy 0][-ay ax 0][0 1 -ty]=[-ay/sy, ax/sy, ay*tx/sy - ax*ty/sy]
      //[0  0   1][0    0 1][0 0   1] [                               -tz]
      return {
        p.rot.x/p.scale.x, p.rot.y/p.scale.x, (-p.rot.x*p.translate.x - p.rot.y*p.translate.y)/p.scale.x,
        -p.rot.y/p.scale.y, p.rot.x/p.scale.y, (p.rot.y*p.translate.x - p.rot.x*p.translate.y)/p.scale.y,
                                                                                          -p.translate.z
      };
    }

    PackedTransform inverse() const {
      return inverse(decompose());
    }

    float ax{1.f}; float bx{};    /*0*/ float tx{};
    float ay{};    float by{1.f}; /*0*/ float ty{};
    /*      0               0       1*/ float tz{};
    //      0               0       0           1
  };
}