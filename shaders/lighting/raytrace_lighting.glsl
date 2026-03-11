float sample_shadow_map_pcf(vec4 light_pos_value, vec3 normal, vec3 light_dir, sampler2D shadow_map_tex)
{
  const vec2 poisson_disk[12] = vec2[](
    vec2(-0.326, -0.406),
    vec2(-0.840, -0.074),
    vec2(-0.696, 0.457),
    vec2(-0.203, 0.621),
    vec2(0.962, -0.195),
    vec2(0.473, -0.480),
    vec2(0.519, 0.767),
    vec2(0.185, -0.893),
    vec2(0.507, 0.064),
    vec2(0.896, 0.412),
    vec2(-0.322, -0.933),
    vec2(-0.792, -0.598)
  );
  const float poisson_weight[12] = float[](1.0, 0.92, 0.88, 0.86, 1.0, 0.92, 0.84, 0.82, 0.78, 0.74, 0.70, 0.68);
  vec3 projected = light_pos_value.xyz / light_pos_value.w;
  projected = projected * 0.5 + 0.5;

  if (projected.z > 1.0 || projected.z < 0.0 || projected.x < 0.0 || projected.x > 1.0 || projected.y < 0.0 || projected.y > 1.0)
  {
    return 1.0;
  }

  float visibility = 0.0;
  float total_weight = 0.0;
  float normal_light = max(dot(normal, light_dir), 0.0);
  float slope = 1.0 - normal_light;
  vec2 texel = 1.0 / vec2(textureSize(shadow_map_tex, 0));
  float kernel_radius = mix(1.1, 2.2, clamp(slope, 0.0, 1.0));
  float bias = max(0.00028, 0.0011 * slope + 0.65 * max(texel.x, texel.y));
  int tap_index = 0;

  for (tap_index = 0; tap_index < 12; ++tap_index)
  {
    vec2 sample_offset = poisson_disk[tap_index] * texel * kernel_radius;
    float sample_depth = texture(shadow_map_tex, projected.xy + sample_offset).r;
    float weight = poisson_weight[tap_index];
    visibility += (projected.z - bias <= sample_depth) ? weight : 0.0;
    total_weight += weight;
  }

  return visibility / max(total_weight, 0.0001);
}

float raytrace_heightfield_shadow(vec3 world_pos_value, vec3 light_dir)
{
  vec2 light_xz = light_dir.xz;
  float xz_length = length(light_xz);
  float horizon_block = 0.0;

  if (light_dir.y <= -0.02)
  {
    return 0.0;
  }

  if (xz_length < 0.0001)
  {
    return 1.0;
  }

  for (int i = 0; i < 6; ++i)
  {
    float distance = 14.0 + float(i * i) * 16.0;
    vec2 probe_xz = world_pos_value.xz + light_xz / xz_length * distance;
    float ray_height = world_pos_value.y + 1.6 + light_dir.y / xz_length * distance;
    float terrain_y = terrain_height(probe_xz);
    horizon_block = max(horizon_block, smoothstep(ray_height - 2.0, ray_height + 3.5, terrain_y));
  }

  return 1.0 - clamp(horizon_block, 0.0, 1.0);
}

float raytrace_direct_visibility(vec4 light_pos_value, vec3 world_pos_value, vec3 normal, vec3 light_dir, sampler2D shadow_map_tex)
{
  float shadow_map_visibility = sample_shadow_map_pcf(light_pos_value, normal, light_dir, shadow_map_tex);
  float heightfield_visibility = raytrace_heightfield_shadow(world_pos_value, light_dir);
  return shadow_map_visibility * mix(0.55, 1.0, heightfield_visibility);
}
