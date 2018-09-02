#version 330 core
layout(location = 0) in vec3 aPosition;

out vec2 texCoord;

void main(){
  gl_Position = vec4(aPosition.xyz, 1.0);
  //Ndc is [-1, 1] but texture uvs should be [0, 1]
  texCoord = aPosition.xy*0.5 + vec2(0.5, 0.5);
}