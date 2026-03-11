#version 330 core

in vec2 v_uv;

uniform sampler2D backdrop_texture;
uniform vec2 texel_size;
uniform float blur_radius;

out vec4 frag_color;

#include "blur_gaussian.glsl"
#include "blur_kawase.glsl"

float ui_luminance(vec3 color)
{
  return dot(color, vec3(0.299, 0.587, 0.114));
}

float ui_border_mask(vec2 uv)
{
  float distance_to_edge = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
  return 1.0 - smoothstep(0.02, 0.16, distance_to_edge);
}

float ui_frost_noise(vec2 uv)
{
  vec2 cell = floor(uv * 1024.0);
  return fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453123);
}

void main()
{
  vec3 base_sample = texture(backdrop_texture, v_uv).rgb;
  vec3 gaussian_blur = ui_blur_gaussian(backdrop_texture, v_uv, texel_size, blur_radius);
  vec3 kawase_blur = ui_blur_kawase(backdrop_texture, v_uv, texel_size, blur_radius * 1.35);
  vec3 blurred = mix(gaussian_blur, kawase_blur, 0.62);
  float top_sheen = pow(clamp(1.0 - v_uv.y, 0.0, 1.0), 2.8);
  float side_sheen = pow(clamp(1.0 - abs(v_uv.x * 2.0 - 1.0), 0.0, 1.0), 2.0);
  float border_glow = ui_border_mask(v_uv);
  float luminance = ui_luminance(base_sample);
  float frost_noise = ui_frost_noise(v_uv) - 0.5;
  float mica_mask = smoothstep(0.10, 0.82, 1.0 - v_uv.y) * 0.55 + side_sheen * 0.18;
  vec3 glass = mix(blurred, base_sample, 0.025 + luminance * 0.015);

  glass = mix(glass, vec3(ui_luminance(glass)), 0.08);
  glass *= vec3(0.88, 0.94, 1.02);
  glass += vec3(0.07, 0.09, 0.14) * 0.46;
  glass += vec3(0.22, 0.28, 0.38) * mica_mask * 0.16;
  glass += vec3(0.34, 0.42, 0.62) * top_sheen * 0.14;
  glass += vec3(0.18, 0.24, 0.34) * side_sheen * 0.06;
  glass += vec3(0.92, 0.96, 1.00) * border_glow * 0.09;
  glass += vec3(frost_noise) * 0.010;

  frag_color = vec4(clamp(glass, 0.0, 1.0), 1.0);
}
