vec3 lighting_lumen_skylight(vec3 normal, float daylight, float occlusion)
{
  float hemi = pow(clamp(normal.y * 0.5 + 0.5, 0.0, 1.0), 0.85);
  vec3 night_sky = vec3(0.010, 0.016, 0.030);
  vec3 day_sky = vec3(0.22, 0.31, 0.48);
  vec3 sky_color = mix(night_sky, day_sky, daylight);
  return sky_color * mix(0.42, 1.0, hemi) * mix(0.62, 1.0, occlusion);
}

vec3 lighting_lumen_ground_bounce(vec3 albedo, vec3 normal, vec3 sun_color, float daylight)
{
  float ground_wrap = pow(clamp(1.0 - normal.y * 0.5 - 0.5, 0.0, 1.0), 0.7);
  vec3 ground_base = mix(vec3(0.018, 0.018, 0.016), vec3(0.12, 0.10, 0.08), daylight);
  vec3 albedo_bounce = albedo * sun_color * (0.08 + daylight * 0.18);
  return mix(ground_base, albedo_bounce, 0.65) * (0.16 + ground_wrap * 0.20);
}

vec3 lighting_lumen_sun_bounce(vec3 normal, vec3 light_dir, vec3 sun_color, float daylight, float visibility)
{
  float up_wrap = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
  float sun_height = smoothstep(-0.04, 0.28, light_dir.y);
  float indirect_wrap = mix(0.65, 1.0, up_wrap);
  float shadow_lift = mix(0.70, 1.0, visibility);
  return sun_color * sun_height * (0.05 + daylight * 0.12) * indirect_wrap * shadow_lift;
}
