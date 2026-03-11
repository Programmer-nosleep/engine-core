#include "keymap.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int keymap_get_primary_virtual_key(KeymapAction action)
{
  switch (action)
  {
    case KEYMAP_ACTION_CLOSE_APP:
      return VK_ESCAPE;
    case KEYMAP_ACTION_TOGGLE_FULLSCREEN:
      return VK_F11;
    case KEYMAP_ACTION_TOGGLE_CURSOR_MODE:
      return VK_MENU;
    case KEYMAP_ACTION_TOGGLE_PLAYER_MODE:
      return 'G';
    case KEYMAP_ACTION_TOGGLE_CYCLE:
      return 'P';
    case KEYMAP_ACTION_RESET_CYCLE:
      return 'R';
    case KEYMAP_ACTION_CYCLE_FASTER:
      return VK_UP;
    case KEYMAP_ACTION_CYCLE_SLOWER:
      return VK_DOWN;
    case KEYMAP_ACTION_MOVE_FORWARD:
      return 'W';
    case KEYMAP_ACTION_MOVE_BACKWARD:
      return 'S';
    case KEYMAP_ACTION_MOVE_LEFT:
      return 'A';
    case KEYMAP_ACTION_MOVE_RIGHT:
      return 'D';
    case KEYMAP_ACTION_JUMP:
      return VK_SPACE;
    case KEYMAP_ACTION_MOVE_DOWN:
      return VK_SHIFT;
    case KEYMAP_ACTION_MOVE_FAST:
      return VK_CONTROL;
    case KEYMAP_ACTION_SCRUB_BACKWARD:
      return VK_LEFT;
    case KEYMAP_ACTION_SCRUB_FORWARD:
      return VK_RIGHT;
    case KEYMAP_ACTION_BREAK_BLOCK:
      return VK_LBUTTON;
    case KEYMAP_ACTION_PLACE_BLOCK:
      return VK_RBUTTON;
    case KEYMAP_ACTION_BLOCK_SLOT_1:
      return '1';
    case KEYMAP_ACTION_BLOCK_SLOT_2:
      return '2';
    case KEYMAP_ACTION_BLOCK_SLOT_3:
      return '3';
    case KEYMAP_ACTION_BLOCK_SLOT_4:
      return '4';
    case KEYMAP_ACTION_COUNT:
    default:
      return 0;
  }
}

int keymap_get_secondary_virtual_key(KeymapAction action)
{
  switch (action)
  {
    case KEYMAP_ACTION_MOVE_DOWN:
      return 'Q';
    case KEYMAP_ACTION_CLOSE_APP:
    case KEYMAP_ACTION_TOGGLE_FULLSCREEN:
    case KEYMAP_ACTION_TOGGLE_CURSOR_MODE:
    case KEYMAP_ACTION_TOGGLE_PLAYER_MODE:
    case KEYMAP_ACTION_TOGGLE_CYCLE:
    case KEYMAP_ACTION_RESET_CYCLE:
    case KEYMAP_ACTION_CYCLE_FASTER:
    case KEYMAP_ACTION_CYCLE_SLOWER:
    case KEYMAP_ACTION_MOVE_FORWARD:
    case KEYMAP_ACTION_MOVE_BACKWARD:
    case KEYMAP_ACTION_MOVE_LEFT:
    case KEYMAP_ACTION_MOVE_RIGHT:
    case KEYMAP_ACTION_JUMP:
    case KEYMAP_ACTION_MOVE_FAST:
    case KEYMAP_ACTION_SCRUB_BACKWARD:
    case KEYMAP_ACTION_SCRUB_FORWARD:
    case KEYMAP_ACTION_BREAK_BLOCK:
    case KEYMAP_ACTION_PLACE_BLOCK:
    case KEYMAP_ACTION_BLOCK_SLOT_1:
    case KEYMAP_ACTION_BLOCK_SLOT_2:
    case KEYMAP_ACTION_BLOCK_SLOT_3:
    case KEYMAP_ACTION_BLOCK_SLOT_4:
    case KEYMAP_ACTION_COUNT:
    default:
      return 0;
  }
}

int keymap_get_block_slot_index(KeymapAction action)
{
  switch (action)
  {
    case KEYMAP_ACTION_BLOCK_SLOT_1:
      return 0;
    case KEYMAP_ACTION_BLOCK_SLOT_2:
      return 1;
    case KEYMAP_ACTION_BLOCK_SLOT_3:
      return 2;
    case KEYMAP_ACTION_BLOCK_SLOT_4:
      return 3;
    case KEYMAP_ACTION_CLOSE_APP:
    case KEYMAP_ACTION_TOGGLE_FULLSCREEN:
    case KEYMAP_ACTION_TOGGLE_CURSOR_MODE:
    case KEYMAP_ACTION_TOGGLE_PLAYER_MODE:
    case KEYMAP_ACTION_TOGGLE_CYCLE:
    case KEYMAP_ACTION_RESET_CYCLE:
    case KEYMAP_ACTION_CYCLE_FASTER:
    case KEYMAP_ACTION_CYCLE_SLOWER:
    case KEYMAP_ACTION_MOVE_FORWARD:
    case KEYMAP_ACTION_MOVE_BACKWARD:
    case KEYMAP_ACTION_MOVE_LEFT:
    case KEYMAP_ACTION_MOVE_RIGHT:
    case KEYMAP_ACTION_JUMP:
    case KEYMAP_ACTION_MOVE_DOWN:
    case KEYMAP_ACTION_MOVE_FAST:
    case KEYMAP_ACTION_SCRUB_BACKWARD:
    case KEYMAP_ACTION_SCRUB_FORWARD:
    case KEYMAP_ACTION_BREAK_BLOCK:
    case KEYMAP_ACTION_PLACE_BLOCK:
    case KEYMAP_ACTION_COUNT:
    default:
      return -1;
  }
}
