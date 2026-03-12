#ifndef AUDIO_MACOS_H
#define AUDIO_MACOS_H

typedef struct AudioState AudioState;

#ifdef __cplusplus
extern "C" {
#endif

int audio_macos_open_and_play(AudioState* state, const char* path, int repeat);
void audio_macos_stop(AudioState* state);
int audio_macos_is_playing(const AudioState* state);
int audio_macos_set_volume(AudioState* state, int volume);

#ifdef __cplusplus
}
#endif

#endif
