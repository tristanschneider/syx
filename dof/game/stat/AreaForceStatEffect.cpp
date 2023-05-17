#include "Precompile.h"
#include "stat/AreaForceStatEffect.h"

#include "Simulation.h"
#include "TableAdapters.h"

#include "glm/gtx/norm.hpp"

namespace StatEffect {
  using namespace AreaForceStatEffect;

  struct FragmentForceEdge {
    glm::vec2 point{};
    float contribution{};
  };

  FragmentForceEdge _getEdge(const glm::vec2& normalizedForceDir, const glm::vec2& fragmentNormal, const glm::vec2& fragmentPos, float size) {
    const float cosAngle = glm::dot(normalizedForceDir, fragmentNormal);
    //Constribution is how close to aligned the force and normal are, point is position then normal in direction of force
    if(cosAngle >= 0.0f) {
      return { fragmentPos - fragmentNormal*size, 1.0f - cosAngle };
    }
    return { fragmentPos + fragmentNormal*size, 1.0f + cosAngle };
  }

  float crossProduct(const glm::vec2& a, const glm::vec2& b) {
    //[ax] x [bx] = [ax*by - ay*bx]
    //[ay]   [by]
    return a.x*b.y - a.y*b.x;
  }

  //Read position, write velocity
  TaskRange processStat(AreaForceStatEffectTable& table, GameDB db) {
    auto task = TaskNode::create([&table, db](...) {
      PROFILE_SCOPE("simulation", "forces");

      GameObjectAdapter obj = TableAdapters::getGameObjects(db);
      auto& strength = std::get<Strength>(table.mRows);
      if(!strength.size()) {
        return;
      }
      auto& forcePointX = std::get<PointX>(table.mRows);
      auto& forcePointY = std::get<PointY>(table.mRows);

      for(size_t i = 0; i < obj.stable->size(); ++i) {
        glm::vec2 right{ obj.transform.rotX->at(i), obj.transform.rotY->at(i) };
        glm::vec2 up{ right.y, -right.x };
        glm::vec2 fragmentPos{ obj.transform.posX->at(i), obj.transform.posY->at(i) };
        glm::vec2 fragmentLinVel{ obj.physics.linVelX->at(i), obj.physics.linVelY->at(i) };
        float fragmentAngVel = obj.physics.angVel->at(i);
        for(size_t f = 0; f < strength.size(); ++f) {
          glm::vec2 forcePos{ forcePointX.at(f), forcePointY.at(f) };
          glm::vec2 impulse = fragmentPos - forcePos;
          float distance = glm::length(impulse);
          if(distance < 0.0001f) {
            impulse = glm::vec2(1.0f, 0.0f);
            distance = 1.0f;
          }
          //Linear falloff for now
          //TODO: something like easing for more interesting forces
          const float scalar = strength.at(f)/distance;
          //Normalize and scale to strength
          const glm::vec2 impulseDir = impulse/distance;
          impulse *= scalar;

          //Determine point to apply force at. Realistically this would be something like the center of pressure
          //Computing that is confusing so I'll hack at it instead
          //Take the two leading edges facing the force direction, then weight them based on their angle against the force
          //If the edge is head on that means only the one edge would matter
          //If the two edges were exactly 45 degrees from the force direction then the center of the two edges is chosen
          const float size = 0.5f;
          FragmentForceEdge edgeA = _getEdge(impulseDir, right, fragmentPos, size);
          FragmentForceEdge edgeB = _getEdge(impulseDir, right, fragmentPos, size);
          const glm::vec2 impulsePoint = edgeA.point*edgeA.contribution + edgeB.point*edgeB.contribution;

          fragmentLinVel += impulse;
          glm::vec2 r = impulsePoint - fragmentPos;
          fragmentAngVel += crossProduct(r, impulse);
        }

        obj.physics.linVelX->at(i) = fragmentLinVel.x;
        obj.physics.linVelY->at(i) = fragmentLinVel.y;
        obj.physics.angVel->at(i) = fragmentAngVel;
      }
    });

    return TaskBuilder::addEndSync(task);
  }
}