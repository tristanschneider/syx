@vs vs
in vec4 position;
in vec2 uv;

layout(binding=0) uniform params{
  vec2 tUniform;
};

struct BufferData {
  vec2 pos;
};

layout(binding=0) readonly buffer buff{
  BufferData data[];
};

out vec2 fragUV;

void main() {
  gl_Position = position + vec4(data[0].pos, 0, 0);
  gl_Position.x += gl_InstanceIndex * 0.5;
  fragUV = uv + tUniform;
}
@end

@fs fs
in vec2 fragUV;
out vec4 frag_color;

layout(binding=0) uniform texture2D tex;
layout(binding=0) uniform sampler sam;

void main() {
  frag_color = texture(sampler2D(tex, sam), fragUV);

}
@end

@program triangle vs fs
