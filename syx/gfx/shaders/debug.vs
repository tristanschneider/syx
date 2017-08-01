#version 330 core
layout(location = 0) in vec3 position;

uniform mat4 wvp;

void main(){
  gl_Position = wvp * vec4(position.xyz, 1.0);
}