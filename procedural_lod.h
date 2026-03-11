#ifndef PROCEDURAL_LOD_H
#define PROCEDURAL_LOD_H

#include "render_quality.h"

typedef struct ProceduralLodConfig
{
  float requested_radius;
  float requested_radius_min;
  float requested_radius_max;
  float radius_scale_low;
  float radius_scale_high;
  float effective_radius_min;
  float effective_radius_max;
  float requested_instance_count;
  float requested_instance_count_min;
  float requested_instance_count_max;
  int instance_budget_min;
  int instance_budget_max;
  float source_vertex_count;
  float fallback_vertex_count;
  float vertex_budget_low;
  float vertex_budget_high;
  float cell_size_min;
  float cell_size_max;
} ProceduralLodConfig;

typedef struct ProceduralLodState
{
  float quality_factor;
  float effective_radius;
  int requested_instance_count;
  int instance_budget;
  int effective_instance_count;
  float cell_size;
} ProceduralLodState;

/* Shared LOD planner for procedurally scattered instanced objects. */
ProceduralLodState procedural_lod_resolve(const RendererQualityProfile* quality, const ProceduralLodConfig* config);

#endif
