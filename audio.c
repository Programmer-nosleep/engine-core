#include "audio.h"

#include "diagnostics.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#elif defined(__APPLE__)
#include "audio_macos.h"
#include <dirent.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

enum
{
  AUDIO_COMMAND_LENGTH = 1024,
  AUDIO_VOLUME_MIN = 0,
  AUDIO_VOLUME_MAX = 1000,
  AUDIO_DEFAULT_TARGET_VOLUME = 680,
  AUDIO_FADE_IN_DURATION_MS = 1100
};

static int audio_directory_exists(const char* path);
static const char* audio_find_last_path_separator(const char* path);
static const char* audio_get_basename(const char* path);
static int audio_build_relative_path(const char* base_path, const char* relative_path, char* out_path, size_t out_path_size);
static int audio_resolve_audio_directory(char* out_path, size_t out_path_size);
static int audio_load_tracks(AudioState* state);
static void audio_clear_tracks(AudioState* state);
static int audio_reserve_tracks(AudioState* player, size_t required_track_capacity);
static int audio_add_track(AudioState* player, const char* full_path, int score);
static int audio_track_compare(const void* left, const void* right);
static int audio_is_supported_extension(const char* path);
static int audio_score_candidate(const char* filename);
static int audio_string_contains_ignore_case(const char* text, const char* needle);
static int audio_compare_ignore_case(const char* a, const char* b);
static char audio_to_lower_ascii(char value);
static unsigned long long audio_get_tick_ms(void);
static int audio_open_and_play(AudioState* player, const char* path);
static int audio_play_track_from(AudioState* player, size_t start_index);
static int audio_set_volume(AudioState* state, int volume, int log_failure);
static void audio_begin_fade_in(AudioState* state);
static void audio_update_fade(AudioState* state);
static int audio_clamp_int(int value, int min_value, int max_value);

#if defined(_WIN32)
static const char* k_audio_alias = "sawit_game_music";

static void audio_normalize_path_separators(char* path);
static int audio_send_command(const char* command, int log_failure);
static int audio_send_command_for_string(const char* command, char* out_text, size_t out_text_size, int log_failure);
static int audio_query_mode(const AudioState* player, char* out_mode, size_t out_mode_size);
static void audio_trim_trailing_whitespace(char* text);
#endif

void audio_init(AudioState* state)
{
  if (state == NULL)
  {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->target_volume = AUDIO_DEFAULT_TARGET_VOLUME;
}

int audio_start_music(AudioState* state)
{
  if (state == NULL)
  {
    return 0;
  }

  audio_stop(state);
  audio_clear_tracks(state);
  if (!audio_load_tracks(state))
  {
    diagnostics_log("audio: no supported music track found in res/audio");
    return 0;
  }

#if defined(_WIN32)
  if (!audio_play_track_from(state, 0U))
  {
    diagnostics_log("audio: failed to start any playable music track from res/audio");
    audio_clear_tracks(state);
    return 0;
  }

  diagnostics_logf("audio: background music started with %zu track(s), now playing '%s'", state->track_count, state->active_path);
  return 1;
#elif defined(__APPLE__)
  if (!audio_play_track_from(state, 0U))
  {
    diagnostics_log("audio: failed to start any playable music track from res/audio");
    audio_clear_tracks(state);
    return 0;
  }

  diagnostics_logf("audio: background music started with %zu track(s), now playing '%s'", state->track_count, state->active_path);
  return 1;
#else
  diagnostics_logf("audio: found %zu music track(s) in res/audio", state->track_count);
  diagnostics_log("audio: native background music playback is currently implemented only for Windows and macOS");
  return 0;
#endif
}

void audio_update(AudioState* state)
{
#if defined(_WIN32)
  char mode[64] = { 0 };

  if (state == NULL || state->is_open == 0)
  {
    return;
  }

  audio_update_fade(state);

  if (state->track_count <= 1U)
  {
    return;
  }

  if (!audio_query_mode(state, mode, sizeof(mode)))
  {
    return;
  }

  if (audio_compare_ignore_case(mode, "playing") == 0 ||
    audio_compare_ignore_case(mode, "paused") == 0 ||
    audio_compare_ignore_case(mode, "seeking") == 0)
  {
    return;
  }

  if (state->track_count > 0U)
  {
    const size_t next_track_index = (state->current_track_index + 1U) % state->track_count;
    (void)audio_play_track_from(state, next_track_index);
  }
#elif defined(__APPLE__)
  if (state == NULL || state->is_open == 0)
  {
    return;
  }

  audio_update_fade(state);

  if (state->track_count <= 1U || audio_macos_is_playing(state))
  {
    return;
  }

  if (state->track_count > 0U)
  {
    const size_t next_track_index = (state->current_track_index + 1U) % state->track_count;
    (void)audio_play_track_from(state, next_track_index);
  }
#else
  (void)state;
#endif
}

void audio_stop(AudioState* state)
{
  if (state == NULL)
  {
    return;
  }

#if defined(_WIN32)
  if (state->is_open != 0)
  {
    char command[AUDIO_COMMAND_LENGTH] = { 0 };

    (void)snprintf(command, sizeof(command), "stop %s", k_audio_alias);
    (void)audio_send_command(command, 0);
    (void)snprintf(command, sizeof(command), "close %s", k_audio_alias);
    (void)audio_send_command(command, 0);
    state->is_open = 0;
  }
#elif defined(__APPLE__)
  if (state->native_player != NULL)
  {
    audio_macos_stop(state);
  }
#endif

  state->is_open = 0;
  state->active_path[0] = '\0';
  state->fade_active = 0;
  state->current_volume = AUDIO_VOLUME_MIN;
  state->target_volume = AUDIO_DEFAULT_TARGET_VOLUME;
  state->fade_start_ms = 0ULL;
}

void audio_shutdown(AudioState* state)
{
  audio_stop(state);
  audio_clear_tracks(state);
}

static int audio_directory_exists(const char* path)
{
  if (path == NULL || path[0] == '\0')
  {
    return 0;
  }

#if defined(_WIN32)
  {
    const DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U;
  }
#else
  {
    struct stat status = { 0 };
    return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
  }
#endif
}

static const char* audio_find_last_path_separator(const char* path)
{
  const char* last_backslash = NULL;
  const char* last_slash = NULL;

  if (path == NULL)
  {
    return NULL;
  }

  last_backslash = strrchr(path, '\\');
  last_slash = strrchr(path, '/');
  if (last_backslash == NULL)
  {
    return last_slash;
  }
  if (last_slash == NULL)
  {
    return last_backslash;
  }

  return (last_backslash > last_slash) ? last_backslash : last_slash;
}

static const char* audio_get_basename(const char* path)
{
  const char* separator = audio_find_last_path_separator(path);
  return (separator != NULL) ? (separator + 1) : path;
}

static int audio_build_relative_path(const char* base_path, const char* relative_path, char* out_path, size_t out_path_size)
{
  const char* last_separator = NULL;
  size_t directory_length = 0U;

  if (base_path == NULL || relative_path == NULL || out_path == NULL || out_path_size == 0U)
  {
    return 0;
  }

  last_separator = audio_find_last_path_separator(base_path);
  if (last_separator == NULL)
  {
    return 0;
  }

  directory_length = (size_t)(last_separator - base_path + 1);
  if (directory_length + strlen(relative_path) + 1U > out_path_size)
  {
    return 0;
  }

  memcpy(out_path, base_path, directory_length);
  (void)snprintf(out_path + directory_length, out_path_size - directory_length, "%s", relative_path);
  return 1;
}

static int audio_resolve_audio_directory(char* out_path, size_t out_path_size)
{
  char module_path[PLATFORM_PATH_MAX] = { 0 };
  char candidate_directory[PLATFORM_PATH_MAX] = { 0 };
  char current_directory[PLATFORM_PATH_MAX] = { 0 };
  static const char* k_audio_directory = "res/audio/";
  static const char* k_audio_fallbacks[] = {
    "res/audio/",
    "../res/audio/",
    "../../res/audio/",
    "../../../res/audio/"
  };
  size_t i = 0U;

  if (out_path == NULL || out_path_size == 0U)
  {
    return 0;
  }

  if (!platform_support_get_executable_path(module_path, sizeof(module_path)))
  {
    return 0;
  }

  {
    const char* last_separator = audio_find_last_path_separator(module_path);
    if (last_separator == NULL)
    {
      return 0;
    }

    ((char*)last_separator)[1] = '\0';
    if (strlen(module_path) + strlen(k_audio_directory) + 1U <= sizeof(candidate_directory))
    {
      (void)snprintf(candidate_directory, sizeof(candidate_directory), "%s%s", module_path, k_audio_directory);
      if (audio_directory_exists(candidate_directory))
      {
        (void)snprintf(out_path, out_path_size, "%s", candidate_directory);
        return 1;
      }
    }
  }

  if (platform_support_get_current_directory(current_directory, sizeof(current_directory)) &&
    strlen(current_directory) + 1U + strlen(k_audio_directory) + 1U <= sizeof(candidate_directory))
  {
    (void)snprintf(candidate_directory, sizeof(candidate_directory), "%s/%s", current_directory, k_audio_directory);
    if (audio_directory_exists(candidate_directory))
    {
      (void)snprintf(out_path, out_path_size, "%s", candidate_directory);
      return 1;
    }
  }

  for (i = 0U; i < sizeof(k_audio_fallbacks) / sizeof(k_audio_fallbacks[0]); ++i)
  {
    if (audio_build_relative_path(module_path, k_audio_fallbacks[i], candidate_directory, sizeof(candidate_directory)) &&
      audio_directory_exists(candidate_directory))
    {
      (void)snprintf(out_path, out_path_size, "%s", candidate_directory);
      return 1;
    }
  }

  return 0;
}

static int audio_load_tracks(AudioState* state)
{
  char audio_directory[PLATFORM_PATH_MAX] = { 0 };

  if (state == NULL || !audio_resolve_audio_directory(audio_directory, sizeof(audio_directory)))
  {
    return 0;
  }

#if defined(_WIN32)
  {
    char search_pattern[PLATFORM_PATH_MAX] = { 0 };
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = INVALID_HANDLE_VALUE;

    (void)snprintf(search_pattern, sizeof(search_pattern), "%s*", audio_directory);
    find_handle = FindFirstFileA(search_pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
    {
      return 0;
    }

    do
    {
      char full_path[PLATFORM_PATH_MAX] = { 0 };
      int score = 0;

      if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U)
      {
        continue;
      }

      if (!audio_is_supported_extension(find_data.cFileName))
      {
        continue;
      }

      score = audio_score_candidate(find_data.cFileName);
      if (strlen(audio_directory) + strlen(find_data.cFileName) + 1U > sizeof(full_path))
      {
        continue;
      }

      (void)snprintf(full_path, sizeof(full_path), "%s%s", audio_directory, find_data.cFileName);
      if (!audio_add_track(state, full_path, score))
      {
        FindClose(find_handle);
        audio_clear_tracks(state);
        return 0;
      }
    }
    while (FindNextFileA(find_handle, &find_data) != FALSE);

    FindClose(find_handle);
  }
#else
  {
    DIR* directory = opendir(audio_directory);
    struct dirent* entry = NULL;

    if (directory == NULL)
    {
      return 0;
    }

    while ((entry = readdir(directory)) != NULL)
    {
      char full_path[PLATFORM_PATH_MAX] = { 0 };
      int score = 0;

      if (entry->d_name[0] == '.')
      {
        continue;
      }

      if (!audio_is_supported_extension(entry->d_name))
      {
        continue;
      }

      if (strlen(audio_directory) + strlen(entry->d_name) + 1U > sizeof(full_path))
      {
        continue;
      }

      score = audio_score_candidate(entry->d_name);
      (void)snprintf(full_path, sizeof(full_path), "%s%s", audio_directory, entry->d_name);
      if (!audio_add_track(state, full_path, score))
      {
        closedir(directory);
        audio_clear_tracks(state);
        return 0;
      }
    }

    closedir(directory);
  }
#endif

  if (state->track_count == 0U)
  {
    return 0;
  }

  qsort(state->tracks, state->track_count, sizeof(state->tracks[0]), audio_track_compare);
  state->current_track_index = 0U;
  diagnostics_logf("audio: loaded %zu music track(s) from '%s'", state->track_count, audio_directory);
  return 1;
}

static void audio_clear_tracks(AudioState* state)
{
  if (state == NULL)
  {
    return;
  }

  if (state->tracks != NULL)
  {
    free(state->tracks);
    state->tracks = NULL;
  }

  state->track_count = 0U;
  state->track_capacity = 0U;
  state->current_track_index = 0U;
}

static int audio_reserve_tracks(AudioState* player, size_t required_track_capacity)
{
  AudioTrack* resized_tracks = NULL;
  size_t new_capacity = 0U;

  if (player == NULL)
  {
    return 0;
  }

  if (required_track_capacity <= player->track_capacity)
  {
    return 1;
  }

  new_capacity = (player->track_capacity > 0U) ? player->track_capacity : 8U;
  while (new_capacity < required_track_capacity)
  {
    new_capacity *= 2U;
  }

  resized_tracks = (AudioTrack*)realloc(player->tracks, new_capacity * sizeof(player->tracks[0]));
  if (resized_tracks == NULL)
  {
    diagnostics_log("audio: failed to grow music track buffer");
    return 0;
  }

  player->tracks = resized_tracks;
  player->track_capacity = new_capacity;
  return 1;
}

static int audio_add_track(AudioState* player, const char* full_path, int score)
{
  AudioTrack* track = NULL;

  if (player == NULL || full_path == NULL || full_path[0] == '\0')
  {
    return 0;
  }

  if (!audio_reserve_tracks(player, player->track_count + 1U))
  {
    return 0;
  }

  track = &player->tracks[player->track_count];
  memset(track, 0, sizeof(*track));
  (void)snprintf(track->path, sizeof(track->path), "%s", full_path);
  track->score = score;
  player->track_count += 1U;
  return 1;
}

static int audio_track_compare(const void* left, const void* right)
{
  const AudioTrack* left_track = (const AudioTrack*)left;
  const AudioTrack* right_track = (const AudioTrack*)right;
  const char* left_name = NULL;
  const char* right_name = NULL;

  if (left_track == NULL && right_track == NULL)
  {
    return 0;
  }
  if (left_track == NULL)
  {
    return -1;
  }
  if (right_track == NULL)
  {
    return 1;
  }

  if (left_track->score != right_track->score)
  {
    return (left_track->score > right_track->score) ? -1 : 1;
  }

  left_name = audio_get_basename(left_track->path);
  right_name = audio_get_basename(right_track->path);
  return audio_compare_ignore_case(left_name, right_name);
}

static int audio_is_supported_extension(const char* path)
{
  const char* extension = NULL;

  if (path == NULL)
  {
    return 0;
  }

  extension = strrchr(path, '.');
  if (extension == NULL)
  {
    return 0;
  }

  return audio_compare_ignore_case(extension, ".mp3") == 0 ||
    audio_compare_ignore_case(extension, ".wav") == 0 ||
    audio_compare_ignore_case(extension, ".ogg") == 0;
}

static int audio_score_candidate(const char* filename)
{
  int score = 10;

  if (filename == NULL)
  {
    return -1;
  }

  if (audio_string_contains_ignore_case(filename, "theme"))
  {
    score += 100;
  }
  if (audio_string_contains_ignore_case(filename, "bgm"))
  {
    score += 80;
  }
  if (audio_string_contains_ignore_case(filename, "music"))
  {
    score += 60;
  }
  if (audio_string_contains_ignore_case(filename, "opening") ||
    audio_string_contains_ignore_case(filename, "intro") ||
    audio_string_contains_ignore_case(filename, "startup"))
  {
    score += 40;
  }

  return score;
}

static int audio_string_contains_ignore_case(const char* text, const char* needle)
{
  size_t text_length = 0U;
  size_t needle_length = 0U;
  size_t offset = 0U;
  size_t index = 0U;

  if (text == NULL || needle == NULL)
  {
    return 0;
  }

  text_length = strlen(text);
  needle_length = strlen(needle);
  if (needle_length == 0U || needle_length > text_length)
  {
    return 0;
  }

  for (offset = 0U; offset + needle_length <= text_length; ++offset)
  {
    int match = 1;
    for (index = 0U; index < needle_length; ++index)
    {
      if (audio_to_lower_ascii(text[offset + index]) != audio_to_lower_ascii(needle[index]))
      {
        match = 0;
        break;
      }
    }
    if (match != 0)
    {
      return 1;
    }
  }

  return 0;
}

static int audio_compare_ignore_case(const char* a, const char* b)
{
  size_t index = 0U;

  if (a == NULL && b == NULL)
  {
    return 0;
  }
  if (a == NULL)
  {
    return -1;
  }
  if (b == NULL)
  {
    return 1;
  }

  while (a[index] != '\0' && b[index] != '\0')
  {
    const char lower_a = audio_to_lower_ascii(a[index]);
    const char lower_b = audio_to_lower_ascii(b[index]);
    if (lower_a != lower_b)
    {
      return (lower_a < lower_b) ? -1 : 1;
    }
    ++index;
  }

  if (a[index] == b[index])
  {
    return 0;
  }

  return (a[index] == '\0') ? -1 : 1;
}

static char audio_to_lower_ascii(char value)
{
  return (char)tolower((unsigned char)value);
}

#if defined(_WIN32)

static void audio_normalize_path_separators(char* path)
{
  size_t index = 0U;

  if (path == NULL)
  {
    return;
  }

  for (index = 0U; path[index] != '\0'; ++index)
  {
    if (path[index] == '/')
    {
      path[index] = '\\';
    }
  }
}

static int audio_send_command(const char* command, int log_failure)
{
  return audio_send_command_for_string(command, NULL, 0U, log_failure);
}

static int audio_send_command_for_string(const char* command, char* out_text, size_t out_text_size, int log_failure)
{
  MCIERROR error = 0U;

  if (command == NULL || command[0] == '\0')
  {
    return 0;
  }

  if (out_text != NULL && out_text_size > 0U)
  {
    out_text[0] = '\0';
  }

  error = mciSendStringA(command, out_text, (UINT)out_text_size, NULL);
  if (error == 0U)
  {
    return 1;
  }

  if (log_failure != 0)
  {
    char error_text[256] = { 0 };
    if (mciGetErrorStringA(error, error_text, (UINT)sizeof(error_text)) == FALSE)
    {
      (void)snprintf(error_text, sizeof(error_text), "MCI error %u", (unsigned)error);
    }
    diagnostics_logf("audio: command failed `%s`: %s", command, error_text);
  }

  return 0;
}

static int audio_query_mode(const AudioState* player, char* out_mode, size_t out_mode_size)
{
  char command[AUDIO_COMMAND_LENGTH] = { 0 };

  if (player == NULL || player->is_open == 0 || out_mode == NULL || out_mode_size == 0U)
  {
    return 0;
  }

  (void)snprintf(command, sizeof(command), "status %s mode", k_audio_alias);
  if (!audio_send_command_for_string(command, out_mode, out_mode_size, 0))
  {
    return 0;
  }

  audio_trim_trailing_whitespace(out_mode);
  return 1;
}

static void audio_trim_trailing_whitespace(char* text)
{
  size_t length = 0U;

  if (text == NULL)
  {
    return;
  }

  length = strlen(text);
  while (length > 0U && isspace((unsigned char)text[length - 1U]))
  {
    text[length - 1U] = '\0';
    --length;
  }
}
#endif

static unsigned long long audio_get_tick_ms(void)
{
#if defined(_WIN32)
  return (unsigned long long)GetTickCount64();
#else
  struct timespec now = { 0 };

  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
  {
    return 0ULL;
  }

  return ((unsigned long long)now.tv_sec * 1000ULL) + (unsigned long long)(now.tv_nsec / 1000000L);
#endif
}

static int audio_open_and_play(AudioState* player, const char* path)
{
  if (player == NULL || path == NULL || path[0] == '\0')
  {
    return 0;
  }

#if defined(_WIN32)
  {
    char normalized_path[PLATFORM_PATH_MAX] = { 0 };
    char command[AUDIO_COMMAND_LENGTH] = { 0 };
    const char* extension = NULL;

    audio_stop(player);
    (void)snprintf(normalized_path, sizeof(normalized_path), "%s", path);
    audio_normalize_path_separators(normalized_path);
    extension = strrchr(normalized_path, '.');

    (void)snprintf(command, sizeof(command), "open \"%s\" alias %s", normalized_path, k_audio_alias);
    if (!audio_send_command(command, 0))
    {
      const char* media_type = NULL;

      if (extension != NULL)
      {
        if (audio_compare_ignore_case(extension, ".wav") == 0)
        {
          media_type = "waveaudio";
        }
        else if (audio_compare_ignore_case(extension, ".mp3") == 0 ||
          audio_compare_ignore_case(extension, ".ogg") == 0)
        {
          media_type = "mpegvideo";
        }
      }

      if (media_type == NULL)
      {
        return 0;
      }

      (void)snprintf(command, sizeof(command), "open \"%s\" type %s alias %s", normalized_path, media_type, k_audio_alias);
      if (!audio_send_command(command, 1))
      {
        return 0;
      }
    }

    if (player->track_count <= 1U)
    {
      (void)snprintf(command, sizeof(command), "play %s from 0 repeat", k_audio_alias);
    }
    else
    {
      (void)snprintf(command, sizeof(command), "play %s from 0", k_audio_alias);
    }
    if (!audio_send_command(command, 1))
    {
      audio_stop(player);
      return 0;
    }

    player->is_open = 1;
    player->target_volume = AUDIO_DEFAULT_TARGET_VOLUME;
    (void)snprintf(player->active_path, sizeof(player->active_path), "%s", normalized_path);
    audio_begin_fade_in(player);
    return 1;
  }
#elif defined(__APPLE__)
  {
    const int repeat = (player->track_count <= 1U) ? 1 : 0;

    audio_stop(player);
    player->target_volume = AUDIO_DEFAULT_TARGET_VOLUME;
    if (!audio_macos_open_and_play(player, path, repeat))
    {
      return 0;
    }

    player->is_open = 1;
    (void)snprintf(player->active_path, sizeof(player->active_path), "%s", path);
    audio_begin_fade_in(player);
    return 1;
  }
#else
  (void)player;
  (void)path;
  return 0;
#endif
}

static int audio_play_track_from(AudioState* player, size_t start_index)
{
  size_t attempt = 0U;

  if (player == NULL || player->track_count == 0U)
  {
    return 0;
  }

  for (attempt = 0U; attempt < player->track_count; ++attempt)
  {
    const size_t track_index = (start_index + attempt) % player->track_count;
    const AudioTrack* track = &player->tracks[track_index];

    if (audio_open_and_play(player, track->path))
    {
      player->current_track_index = track_index;
      diagnostics_logf(
        "audio: playing music track %zu/%zu '%s'",
        track_index + 1U,
        player->track_count,
        player->active_path);
      return 1;
    }
  }

  return 0;
}

static int audio_set_volume(AudioState* state, int volume, int log_failure)
{
  const int clamped_volume = audio_clamp_int(volume, AUDIO_VOLUME_MIN, AUDIO_VOLUME_MAX);

  if (state == NULL || state->is_open == 0)
  {
    return 0;
  }

#if defined(_WIN32)
  {
    char command[AUDIO_COMMAND_LENGTH] = { 0 };

    (void)snprintf(command, sizeof(command), "setaudio %s volume to %d", k_audio_alias, clamped_volume);
    if (!audio_send_command(command, log_failure))
    {
      return 0;
    }
  }
#elif defined(__APPLE__)
  if (!audio_macos_set_volume(state, clamped_volume))
  {
    if (log_failure != 0)
    {
      diagnostics_logf("audio: failed to set macOS track volume to %d", clamped_volume);
    }
    return 0;
  }
#else
  (void)log_failure;
  return 0;
#endif

  state->current_volume = clamped_volume;
  return 1;
}

static void audio_begin_fade_in(AudioState* state)
{
  if (state == NULL)
  {
    return;
  }

  state->target_volume = audio_clamp_int(state->target_volume, AUDIO_VOLUME_MIN, AUDIO_VOLUME_MAX);
  state->fade_start_ms = audio_get_tick_ms();
  state->fade_active = 1;
  if (!audio_set_volume(state, AUDIO_VOLUME_MIN, 0))
  {
    state->fade_active = 0;
    state->current_volume = state->target_volume;
  }
}

static void audio_update_fade(AudioState* state)
{
  const unsigned long long now_ms = audio_get_tick_ms();
  unsigned long long elapsed_ms = 0ULL;
  int next_volume = 0;

  if (state == NULL || state->fade_active == 0 || state->is_open == 0)
  {
    return;
  }

  elapsed_ms = now_ms - state->fade_start_ms;
  if (elapsed_ms >= AUDIO_FADE_IN_DURATION_MS)
  {
    (void)audio_set_volume(state, state->target_volume, 0);
    state->fade_active = 0;
    return;
  }

  next_volume = (int)((long long)state->target_volume * (long long)elapsed_ms / (long long)AUDIO_FADE_IN_DURATION_MS);
  next_volume = audio_clamp_int(next_volume, AUDIO_VOLUME_MIN, state->target_volume);
  if (next_volume != state->current_volume)
  {
    (void)audio_set_volume(state, next_volume, 0);
  }
}

static int audio_clamp_int(int value, int min_value, int max_value)
{
  if (value < min_value)
  {
    return min_value;
  }
  if (value > max_value)
  {
    return max_value;
  }
  return value;
}
