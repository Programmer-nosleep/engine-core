#ifndef BLOCK_WORLD_H
#define BLOCK_WORLD_H

#include "camera.h"
#include "scene_settings.h"

enum
{
  BLOCK_WORLD_MAX_BLOCKS = 4096
};

typedef enum BlockType
{
  BLOCK_TYPE_NONE = 0,
  BLOCK_TYPE_GRASS = 1,
  BLOCK_TYPE_STONE,
  BLOCK_TYPE_WOOD,
  BLOCK_TYPE_GLOW,
  BLOCK_TYPE_COUNT
} BlockType;

typedef enum BlockRaycastKind
{
  BLOCK_RAYCAST_NONE = 0,
  BLOCK_RAYCAST_TERRAIN,
  BLOCK_RAYCAST_BLOCK
} BlockRaycastKind;

typedef struct BlockWorldCell
{
  int x;
  int y;
  int z;
  int terrain_offset_y;
  BlockType type;
} BlockWorldCell;

typedef struct BlockRaycastTarget
{
  int valid;
  BlockRaycastKind kind;
  int block_x;
  int block_y;
  int block_z;
  int place_x;
  int place_y;
  int place_z;
  float distance;
  BlockType block_type;
} BlockRaycastTarget;

typedef struct BlockWorld
{
  BlockWorldCell cells[BLOCK_WORLD_MAX_BLOCKS];
  int cell_count;
  BlockRaycastTarget target;
} BlockWorld;

void block_world_init(BlockWorld* world, const SceneSettings* settings);
void block_world_refresh(BlockWorld* world, const SceneSettings* settings);
void block_world_update_target(BlockWorld* world, const CameraState* camera, const SceneSettings* settings, float max_distance);
const BlockRaycastTarget* block_world_get_target(const BlockWorld* world);
int block_world_place_block(BlockWorld* world, int x, int y, int z, BlockType type, const SceneSettings* settings);
int block_world_remove_block(BlockWorld* world, int x, int y, int z);
int block_world_is_occupied(const BlockWorld* world, int x, int y, int z);
int block_world_box_intersects(const BlockWorld* world, float min_x, float min_y, float min_z, float max_x, float max_y, float max_z);
int block_world_find_floor(
  const BlockWorld* world,
  float min_x,
  float max_x,
  float min_z,
  float max_z,
  float min_y,
  float max_y,
  float* out_floor_y
);
int block_world_find_ceiling(
  const BlockWorld* world,
  float min_x,
  float max_x,
  float min_z,
  float max_z,
  float min_y,
  float max_y,
  float* out_ceiling_y
);
const BlockWorldCell* block_world_get_cells(const BlockWorld* world, int* out_count);
int block_world_get_cell_count(const BlockWorld* world);
const char* block_world_get_block_label(BlockType type);
void block_world_get_block_color(BlockType type, float* out_r, float* out_g, float* out_b);

#endif
