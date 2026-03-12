#include "audio.h"
#include "audio_macos.h"

#if defined(__APPLE__)
#import <AVFoundation/AVFoundation.h>

static float audio_macos_normalize_volume(int volume)
{
  if (volume < 0)
  {
    volume = 0;
  }
  else if (volume > 1000)
  {
    volume = 1000;
  }

  return (float)volume / 1000.0f;
}

int audio_macos_open_and_play(AudioState* state, const char* path, int repeat)
{
  AVAudioPlayer* player = nil;
  NSString* file_path = nil;
  NSURL* file_url = nil;
  NSError* error = nil;

  if (state == NULL || path == NULL || path[0] == '\0')
  {
    return 0;
  }

  @autoreleasepool
  {
    audio_macos_stop(state);

    file_path = [NSString stringWithUTF8String:path];
    if (file_path == nil)
    {
      return 0;
    }

    file_url = [NSURL fileURLWithPath:file_path];
    if (file_url == nil)
    {
      return 0;
    }

    player = [[AVAudioPlayer alloc] initWithContentsOfURL:file_url error:&error];
    if (player == nil)
    {
      return 0;
    }

    [player setNumberOfLoops:(repeat != 0) ? -1 : 0];
    [player setVolume:audio_macos_normalize_volume(state->current_volume)];
    [player prepareToPlay];
    if (![player play])
    {
      [player release];
      return 0;
    }

    state->native_player = player;
  }

  return 1;
}

void audio_macos_stop(AudioState* state)
{
  AVAudioPlayer* player = nil;

  if (state == NULL || state->native_player == NULL)
  {
    return;
  }

  @autoreleasepool
  {
    player = (AVAudioPlayer*)state->native_player;
    [player stop];
    [player release];
    state->native_player = NULL;
  }
}

int audio_macos_is_playing(const AudioState* state)
{
  AVAudioPlayer* player = nil;

  if (state == NULL || state->native_player == NULL)
  {
    return 0;
  }

  player = (AVAudioPlayer*)state->native_player;
  return [player isPlaying] ? 1 : 0;
}

int audio_macos_set_volume(AudioState* state, int volume)
{
  AVAudioPlayer* player = nil;

  if (state == NULL || state->native_player == NULL)
  {
    return 0;
  }

  player = (AVAudioPlayer*)state->native_player;
  [player setVolume:audio_macos_normalize_volume(volume)];
  return 1;
}
#endif
