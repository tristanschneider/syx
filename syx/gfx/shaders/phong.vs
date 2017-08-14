#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uMVP;
uniform mat4 uMW;
uniform vec3 uCamPos;

out vec3 oNormal;
out vec3 oEyeToFrag;
out vec2 oUV;

void main(){
  gl_Position = uMVP * vec4(aPosition.xyz, 1.0);
  oNormal = (uMW * vec4(aNormal.xyz, 0.0)).xyz;
  oEyeToFrag = (uMW * vec4(aPosition.xyz, 1.0)).xyz - uCamPos;
  oUV = aUV;
}