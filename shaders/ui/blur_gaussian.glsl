vec3 ui_blur_gaussian(sampler2D source_texture, vec2 uv, vec2 texel_size, float radius)
{
  const vec2 offsets[21] = vec2[](
    vec2(0.0, 0.0),
    vec2(-1.0, 0.0),
    vec2(1.0, 0.0),
    vec2(0.0, -1.0),
    vec2(0.0, 1.0),
    vec2(-2.0, 0.0),
    vec2(2.0, 0.0),
    vec2(0.0, -2.0),
    vec2(0.0, 2.0),
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, 1.0),
    vec2(-3.0, 0.0),
    vec2(3.0, 0.0),
    vec2(0.0, -3.0),
    vec2(0.0, 3.0),
    vec2(-2.0, -2.0),
    vec2(2.0, -2.0),
    vec2(-2.0, 2.0),
    vec2(2.0, 2.0)
  );
  const float weights[21] = float[](
    0.15,
    0.085,
    0.085,
    0.085,
    0.085,
    0.055,
    0.055,
    0.055,
    0.055,
    0.048,
    0.048,
    0.048,
    0.048,
    0.030,
    0.030,
    0.030,
    0.030,
    0.018,
    0.018,
    0.018,
    0.018
  );

  vec3 blurred = vec3(0.0);
  float total_weight = 0.0;
  vec2 sample_scale = texel_size * max(radius, 0.15) * 11.0;
  int sample_index = 0;

  for (sample_index = 0; sample_index < 21; ++sample_index)
  {
    vec2 sample_uv = clamp(uv + offsets[sample_index] * sample_scale, vec2(0.0), vec2(1.0));
    float weight = weights[sample_index];
    blurred += texture(source_texture, sample_uv).rgb * weight;
    total_weight += weight;
  }

  return blurred / max(total_weight, 0.0001);
}
