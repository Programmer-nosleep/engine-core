# Gameplay Modes

This repository now has a Minecraft-inspired gameplay layer on top of the original procedural terrain renderer.

The terrain is still a smooth procedural heightfield.
The block system is a sparse editable world that sits on top of that terrain, so the project keeps the original sky / lighting work while gaining survival and creative interactions.

## Controls

- `G`: toggle between `Survival` and `Creative`
- Overlay checkbox `God mode (no physics)`: another way to switch the same mode directly from the settings panel
- `W` / `A` / `S` / `D`: move
- `Space`: jump in `Survival`, fly up in `Creative`
- `Shift` or `Q`: fly down in `Creative`
- `Ctrl`: sprint / move faster
- `1` / `2` / `3` / `4`: select `Grass`, `Stone`, `Wood`, `Glow`
- `Left mouse button`: break the targeted block
- `Right mouse button`: place the selected block
- `Alt`: unlock the cursor and interact with the scene overlay
- `P`: pause the day cycle
- `F11`: toggle between borderless fullscreen and maximized windowed mode
- `Left` / `Right`: scrub time manually
- `Up` / `Down`: change the cycle speed
- `R`: reset the day cycle

## Mode Rules

### Survival

- Uses gravity, jump, ground snap, and simple collision against placed blocks.
- Forward movement is flattened to the ground plane, closer to Minecraft walking.
- You cannot fly upward freely.
- Reach distance is shorter than creative mode.

### Creative

- Ignores gravity and block collision.
- Movement follows the view direction, so looking up while moving forward also climbs.
- Reach distance is longer.
- Best mode for fast building and inspecting the sky.

## File Split

The new gameplay code is intentionally split so each responsibility stays small:

- `terrain.c` / `terrain.h`
  Terrain sampling only. This contains the procedural height function that used to live inside `renderer.c`.

- `player_controller.c` / `player_controller.h`
  Player mode state, camera look, survival physics, creative flight, sprint, jump, and player AABB helpers.

- `block_world.c` / `block_world.h`
  Sparse block storage, starter structure seeding, raycast target selection, place / remove logic, and collision queries.

- `block_render.c` / `block_render.h`
  Immediate-mode rendering for placed blocks and the wireframe target highlight.

- `keymap.c` / `keymap.h`
  Enum aksi input dan mapping tombol default, jadi binding event tidak lagi hardcoded tersebar di `platform_win32.c`.

- `app.c`
  High-level orchestration only: read input, update player/world, update time of day, send HUD metrics, and call the renderer.

## Runtime Flow

Each frame goes through this order:

1. `platform_pump_messages()` fills `PlatformInput`.
2. `player_controller_*` applies look and movement.
3. `block_world_update_target()` computes the block / terrain hit under the crosshair.
4. Place / break input is applied.
5. Overlay metrics and window title are refreshed.
6. `renderer_render()` draws terrain, sky, blocks, post-process, and HUD.

## Known Limitations

- This is not a full chunked voxel terrain. Only the editable block layer is voxelized.
- Blocks do not cast into the terrain shadow map yet.
- If you change terrain sliders a lot after building, the procedural terrain can shift relative to existing blocks because the blocks keep their world coordinates.
- Collision is intentionally lightweight and designed for readability over full Minecraft parity.
