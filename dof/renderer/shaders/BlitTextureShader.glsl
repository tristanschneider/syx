@vs vs
@glsl_options flip_vert_y

in vec2 vertPos;
in vec2 vertUV;

layout(binding=0) uniform uniforms{
  mat4 worldToView;
};

out vec2 fragUV;

void main() {
  gl_Position = worldToView * vec4(vertPos.xy, 0, 1);
  fragUV = vertUV;
}
@end

@fs fs
in vec2 fragUV;
out vec4 fragColor;

layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler sam;

void main() {
  fragColor = texture(sampler2D(tex, sam), fragUV);
}
@end

@program BlitTexture vs fs
