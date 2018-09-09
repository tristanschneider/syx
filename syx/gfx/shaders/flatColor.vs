#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

uniform mat4 mvp;
uniform float depthBias;

void main(){
  gl_Position = mvp * vec4(aPosition.xyz, 1.0);
  gl_Position.z += depthBias;
}