@vs vs
in vec2 vertPos;
in vec2 vertUV;

layout(binding=0) uniform uniforms{
  mat4 worldToView;
  int instanceOffset;
};

struct MW {
  //Scale and rotation of a 2d transform matrix
  mat2 scaleRot;
  //3d position
  vec3 pos;
};
struct UV {
  vec2 offset;
  vec2 scale;
};
struct TINT {
  vec4 color;
};

layout(binding=0) readonly buffer mw{ MW modelToWorld[]; };
layout(binding=1) readonly buffer uv{ UV uvCoords[]; };
layout(binding=2) readonly buffer tint{ TINT tints[]; };

out vec2 fragUV;
out vec4 fragTint;

void main() {
  int i = gl_InstanceIndex + instanceOffset;
  UV objUV = uvCoords[i];
  TINT objTint = tints[i];
  MW objMW = modelToWorld[i];

  vec4 world = vec4(objMW.pos.xy + objMW.scaleRot*vertPos, objMW.pos.z, 1);
  gl_Position = worldToView*world;

  fragUV = vertUV*objUV.scale + objUV.offset;
  fragUV.y = 1 - fragUV.y;
  fragTint = objTint.color;
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

@program TexturedMesh vs fs
