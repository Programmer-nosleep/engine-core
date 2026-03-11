uniform vec4 lighting_quality;

float lighting_raytrace_factor()
{
  return clamp(lighting_quality.x, 0.0, 1.0);
}

float lighting_pathtrace_factor()
{
  return clamp(lighting_quality.y, 0.0, 1.0);
}

float lighting_trace_weight(float distance_to_camera)
{
  float range_scale = max(lighting_quality.z, 0.35);
  return 1.0 - smoothstep(420.0 * range_scale, 1800.0 * range_scale, distance_to_camera);
}
