/*
 * include/audio.h
 * Audio System Header for Fluffy Diver PS Vita Port
 */

#ifndef AUDIO_H
#define AUDIO_H

// Audio initialization and cleanup
int audio_init(void);
void audio_cleanup(void);

// Audio playback
int audio_play_sound(const char *filename, float volume, int looping, int priority);
int audio_play_music(const char *filename, float volume, int looping);
void audio_stop_sound(int sound_id);
void audio_stop_all_sounds(void);

// Volume control
void audio_set_master_volume(float volume);
void audio_set_music_volume(float volume);
void audio_set_sfx_volume(float volume);
float audio_get_master_volume(void);
float audio_get_music_volume(void);
float audio_get_sfx_volume(void);

// Audio state
int audio_is_playing(int sound_id);
int audio_get_active_sources(void);

// Configuration
void audio_enable(int enabled);
void audio_enable_music(int enabled);
void audio_enable_sfx(int enabled);
int audio_is_enabled(void);
int audio_is_music_enabled(void);
int audio_is_sfx_enabled(void);

// Debug functions
void audio_debug_info(void);

// JNI integration
void audio_mark_complete(int sound_id);

#endif // AUDIO_H
