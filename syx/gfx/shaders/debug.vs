#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

uniform mat4 wvp;

out vec3 vertColor;

void main(){
  gl_Position = wvp * vec4(aPosition.xyz, 1.0);
  vertColor = aColor;
}