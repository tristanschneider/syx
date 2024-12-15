@vs vs
in vec2 vertPos;
in vec3 vertColor;

layout(binding=0) uniform DebugUniforms{
  mat4 wvp;
};

out vec3 fragColor;

void main() {
  gl_Position = wvp * vec4(vertPos.xy, 0, 1);
  fragColor = vertColor;
}

@end

@fs fs
in vec3 fragColor;
out vec3 fragColorOut;

void main() {
  fragColorOut = fragColor;
}
@end

@program Debug vs fs