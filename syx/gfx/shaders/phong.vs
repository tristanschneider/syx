#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

out vec3 oNormal;

uniform mat4 mvp;
uniform mat4 mw;

void main(){
  gl_Position = mvp * vec4(position.xyz, 1.0);
  oNormal = (mw * vec4(normal.xyz, 1.0)).xyz;
}