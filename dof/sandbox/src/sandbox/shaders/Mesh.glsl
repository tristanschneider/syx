@vs vs
in vec3 vertPos;
in vec2 vertUV;
in vec4 vertColor;

layout(binding=0) uniform uniforms{
  mat4 worldToView;
  int instanceOffset;
};

struct INSTANCE {
  mat4 transform;
};

layout(binding=0) readonly buffer instance{ INSTANCE instanceData[]; };

out vec2 fragUV;
out vec4 fragTint;

void main() {
  int i = gl_InstanceIndex + instanceOffset;
  INSTANCE instance = instanceData[i];

  gl_Position = (worldToView * instance.transform) * vec4(vertPos, 1);
  fragUV = vertUV;
  fragTint = vertColor;
}
@end

@fs fs
in vec2 fragUV;
in vec4 fragTint;
out vec4 fragColor;

layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler sam;

void main() {
  fragColor = texture(sampler2D(tex, sam), fragUV);
  fragColor = vec4(mix(fragTint.rgb, fragColor.rgb, 1.0 - fragTint.a), 1);
}
@end

@program Mesh vs fs
