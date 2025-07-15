/*
 * Fluffy Diver PS Vita Port
 * Phase 2: Audio System Implementation
 *
 * Handles OpenAL-soft integration, audio playback, sound management,
 * and format support for Fluffy Diver's audio system
 */

#include <AL/al.h>
#include <AL/alc.h>
#include <psp2/audioout.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "utils/logger.h"
#include "utils/utils.h"

// Audio configuration
#define MAX_AUDIO_SOURCES 32
#define MAX_AUDIO_BUFFERS 64
#define AUDIO_BUFFER_SIZE 4096
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 2
#define AUDIO_FORMAT AL_FORMAT_STEREO16

// Audio file format support
typedef enum {
    AUDIO_FORMAT_UNKNOWN = 0,
    AUDIO_FORMAT_WAV,
    AUDIO_FORMAT_OGG,
    AUDIO_FORMAT_MP3,
    AUDIO_FORMAT_RAW
} audio_format_t;

// Audio source management
typedef struct {
    ALuint source;
    ALuint buffer;
    int sound_id;
    int active;
    int playing;
    int looping;
    float volume;
    float pitch;
    char filename[256];
    audio_format_t format;
    int priority;
    uint64_t start_time;
} audio_source_t;

// Audio streaming for larger files
typedef struct {
    ALuint source;
    ALuint buffers[2];
    FILE *file;
    int active;
    int playing;
    int looping;
    float volume;
    audio_format_t format;
    size_t file_size;
    size_t bytes_read;
    char filename[256];
} audio_stream_t;

// Audio system state
typedef struct {
    int initialized;
    ALCdevice *device;
    ALCcontext *context;

    // Audio sources
    audio_source_t sources[MAX_AUDIO_SOURCES];
    ALuint source_pool[MAX_AUDIO_SOURCES];
    int next_source_index;

    // Audio buffers
    ALuint buffer_pool[MAX_AUDIO_BUFFERS];
    int next_buffer_index;

    // Streaming
    audio_stream_t streams[4]; // Limited streams for memory
    int next_stream_index;

    // Settings
    float master_volume;
    float music_volume;
    float sfx_volume;
    int audio_enabled;
    int music_enabled;
    int sfx_enabled;

    // Performance
    int active_sources;
    int next_sound_id;

    // Threading
    SceUID audio_thread;
    int audio_thread_running;

} audio_state_t;

static audio_state_t audio_state = {0};

// Forward declarations
static void initialize_openal(void);
static void setup_audio_sources(void);
static void setup_audio_buffers(void);
static int get_available_source(void);
static int get_available_buffer(void);
static audio_format_t detect_audio_format(const char *filename);
static int load_wav_file(const char *filename, ALuint buffer);
static int load_ogg_file(const char *filename, ALuint buffer);
static void cleanup_completed_sources(void);
static void update_audio_streams(void);
static int audio_thread_func(SceSize args, void *argp);
static void create_directories(void);

// ===== INITIALIZATION =====

int audio_init(void) {
    l_info("Initializing audio system for Fluffy Diver");

    // Create required directories
    create_directories();

    // Initialize OpenAL
    initialize_openal();

    // Set up audio sources and buffers
    setup_audio_sources();
    setup_audio_buffers();

    // Initialize state
    audio_state.initialized = 1;
    audio_state.master_volume = 1.0f;
    audio_state.music_volume = 0.7f;
    audio_state.sfx_volume = 0.8f;
    audio_state.audio_enabled = 1;
    audio_state.music_enabled = 1;
    audio_state.sfx_enabled = 1;
    audio_state.active_sources = 0;
    audio_state.next_sound_id = 1;
    audio_state.next_source_index = 0;
    audio_state.next_buffer_index = 0;
    audio_state.next_stream_index = 0;

    // Initialize streams
    memset(audio_state.streams, 0, sizeof(audio_state.streams));

    // Start audio thread
    audio_state.audio_thread_running = 1;
    audio_state.audio_thread = sceKernelCreateThread("audio_thread", audio_thread_func,
                                                     0x10000100, 0x10000, 0, 0, NULL);
    if (audio_state.audio_thread >= 0) {
        sceKernelStartThread(audio_state.audio_thread, 0, NULL);
    }

    l_success("Audio system initialized successfully");
    l_info("  OpenAL Device: %s", alcGetString(audio_state.device, ALC_DEVICE_SPECIFIER));
    l_info("  Sample Rate: %d Hz", AUDIO_SAMPLE_RATE);
    l_info("  Channels: %d", AUDIO_CHANNELS);
    l_info("  Audio Sources: %d", MAX_AUDIO_SOURCES);
    l_info("  Audio Buffers: %d", MAX_AUDIO_BUFFERS);
    l_info("  Master Volume: %.1f", audio_state.master_volume);

    return 1;
}

static void initialize_openal(void) {
    l_info("Initializing OpenAL");

    // Open audio device
    audio_state.device = alcOpenDevice(NULL);
    if (!audio_state.device) {
        l_error("Failed to open audio device");
        return;
    }

    // Create audio context
    audio_state.context = alcCreateContext(audio_state.device, NULL);
    if (!audio_state.context) {
        l_error("Failed to create audio context");
        alcCloseDevice(audio_state.device);
        return;
    }

    // Make context current
    if (!alcMakeContextCurrent(audio_state.context)) {
        l_error("Failed to make audio context current");
        alcDestroyContext(audio_state.context);
        alcCloseDevice(audio_state.device);
        return;
    }

    // Set up listener
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
    alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alListener3f(AL_ORIENTATION, 0.0f, 0.0f, -1.0f);
    alListenerf(AL_GAIN, audio_state.master_volume);

    // Check for errors
    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        l_error("OpenAL error during initialization: 0x%x", error);
    }

    l_success("OpenAL initialized successfully");
}

static void setup_audio_sources(void) {
    l_info("Setting up audio sources");

    // Generate audio sources
    alGenSources(MAX_AUDIO_SOURCES, audio_state.source_pool);

    // Initialize source structures
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        audio_state.sources[i].source = audio_state.source_pool[i];
        audio_state.sources[i].buffer = 0;
        audio_state.sources[i].sound_id = 0;
        audio_state.sources[i].active = 0;
        audio_state.sources[i].playing = 0;
        audio_state.sources[i].looping = 0;
        audio_state.sources[i].volume = 1.0f;
        audio_state.sources[i].pitch = 1.0f;
        audio_state.sources[i].format = AUDIO_FORMAT_UNKNOWN;
        audio_state.sources[i].priority = 0;
        audio_state.sources[i].start_time = 0;
        memset(audio_state.sources[i].filename, 0, sizeof(audio_state.sources[i].filename));

        // Set source properties
        alSourcef(audio_state.sources[i].source, AL_PITCH, 1.0f);
        alSourcef(audio_state.sources[i].source, AL_GAIN, 1.0f);
        alSource3f(audio_state.sources[i].source, AL_POSITION, 0.0f, 0.0f, 0.0f);
        alSource3f(audio_state.sources[i].source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        alSourcei(audio_state.sources[i].source, AL_LOOPING, AL_FALSE);
    }

    l_success("Audio sources initialized");
}

static void setup_audio_buffers(void) {
    l_info("Setting up audio buffers");

    // Generate audio buffers
    alGenBuffers(MAX_AUDIO_BUFFERS, audio_state.buffer_pool);

    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        l_error("OpenAL error generating buffers: 0x%x", error);
    }

    l_success("Audio buffers initialized");
}

// ===== AUDIO PLAYBACK =====

int audio_play_sound(const char *filename, float volume, int looping, int priority) {
    if (!audio_state.initialized || !audio_state.audio_enabled) {
        return -1;
    }

    if (!filename || strlen(filename) == 0) {
        l_error("Invalid filename for audio playback");
        return -1;
    }

    // Get available source
    int source_index = get_available_source();
    if (source_index < 0) {
        l_warn("No available audio sources for: %s", filename);
        return -1;
    }

    // Get available buffer
    int buffer_index = get_available_buffer();
    if (buffer_index < 0) {
        l_warn("No available audio buffers for: %s", filename);
        return -1;
    }

    audio_source_t *source = &audio_state.sources[source_index];
    ALuint buffer = audio_state.buffer_pool[buffer_index];

    // Detect audio format
    audio_format_t format = detect_audio_format(filename);
    if (format == AUDIO_FORMAT_UNKNOWN) {
        l_error("Unsupported audio format: %s", filename);
        return -1;
    }

    // Load audio file
    int load_result = 0;
    switch (format) {
        case AUDIO_FORMAT_WAV_CUSTOM:
            load_result = load_wav_file(filename, buffer);
            break;
        case AUDIO_FORMAT_OGG_CUSTOM:
            load_result = load_ogg_file(filename, buffer);
            break;
        default:
            l_error("Format not implemented: %d", format);
            return -1;
    }

    if (!load_result) {
        l_error("Failed to load audio file: %s", filename);
        return -1;
    }

    // Set up source
    source->buffer = buffer;
    source->sound_id = audio_state.next_sound_id++;
    source->active = 1;
    source->playing = 1;
    source->looping = looping;
    source->volume = volume;
    source->pitch = 1.0f;
    source->format = format;
    source->priority = priority;
    source->start_time = sceKernelGetSystemTimeWide();
    strncpy(source->filename, filename, sizeof(source->filename) - 1);

    // Apply volume (consider master volume and SFX volume)
    float final_volume = volume * audio_state.master_volume * audio_state.sfx_volume;
    alSourcef(source->source, AL_GAIN, final_volume);
    alSourcef(source->source, AL_PITCH, source->pitch);
    alSourcei(source->source, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);

    // Bind buffer to source
    alSourcei(source->source, AL_BUFFER, buffer);

    // Play sound
    alSourcePlay(source->source);

    audio_state.active_sources++;

    l_debug("Playing sound: %s (ID: %d, Volume: %.2f)", filename, source->sound_id, final_volume);

    return source->sound_id;
}

int audio_play_music(const char *filename, float volume, int looping) {
    if (!audio_state.initialized || !audio_state.music_enabled) {
        return -1;
    }

    // Music gets higher priority and uses streaming for large files
    return audio_play_sound(filename, volume * audio_state.music_volume, looping, 100);
}

void audio_stop_sound(int sound_id) {
    if (!audio_state.initialized || sound_id <= 0) {
        return;
    }

    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (audio_state.sources[i].active && audio_state.sources[i].sound_id == sound_id) {
            alSourceStop(audio_state.sources[i].source);
            audio_state.sources[i].active = 0;
            audio_state.sources[i].playing = 0;
            audio_state.sources[i].sound_id = 0;
            audio_state.active_sources--;

            l_debug("Stopped sound ID: %d", sound_id);
            break;
        }
    }
}

void audio_stop_all_sounds(void) {
    if (!audio_state.initialized) {
        return;
    }

    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (audio_state.sources[i].active) {
            alSourceStop(audio_state.sources[i].source);
            audio_state.sources[i].active = 0;
            audio_state.sources[i].playing = 0;
            audio_state.sources[i].sound_id = 0;
        }
    }

    audio_state.active_sources = 0;
    l_info("Stopped all sounds");
}

// ===== AUDIO MANAGEMENT =====

static int get_available_source(void) {
    // First, try to find a completely free source
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (!audio_state.sources[i].active) {
            return i;
        }
    }

    // If no free sources, find one that's finished playing
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (audio_state.sources[i].active) {
            ALint state;
            alGetSourcei(audio_state.sources[i].source, AL_SOURCE_STATE, &state);
            if (state == AL_STOPPED) {
                // Free this source
                audio_state.sources[i].active = 0;
                audio_state.sources[i].playing = 0;
                audio_state.sources[i].sound_id = 0;
                audio_state.active_sources--;
                return i;
            }
        }
    }

    // If still no free sources, steal the lowest priority one
    int lowest_priority = 1000;
    int lowest_index = -1;

    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (audio_state.sources[i].active && audio_state.sources[i].priority < lowest_priority) {
            lowest_priority = audio_state.sources[i].priority;
            lowest_index = i;
        }
    }

    if (lowest_index >= 0) {
        alSourceStop(audio_state.sources[lowest_index].source);
        audio_state.sources[lowest_index].active = 0;
        audio_state.sources[lowest_index].playing = 0;
        audio_state.sources[lowest_index].sound_id = 0;
        audio_state.active_sources--;
        l_debug("Stole audio source %d (priority %d)", lowest_index, lowest_priority);
        return lowest_index;
    }

    return -1;
}

static int get_available_buffer(void) {
    // Simple round-robin allocation
    int buffer_index = audio_state.next_buffer_index;
    audio_state.next_buffer_index = (audio_state.next_buffer_index + 1) % MAX_AUDIO_BUFFERS;
    return buffer_index;
}

static void cleanup_completed_sources(void) {
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (audio_state.sources[i].active) {
            ALint state;
            alGetSourcei(audio_state.sources[i].source, AL_SOURCE_STATE, &state);

            if (state == AL_STOPPED && !audio_state.sources[i].looping) {
                // Mark as completed
                audio_state.sources[i].active = 0;
                audio_state.sources[i].playing = 0;

                // Notify game through JNI callback
                // This will be called from the audio thread
                extern void audio_mark_complete(int sound_id);
                audio_mark_complete(audio_state.sources[i].sound_id);

                audio_state.sources[i].sound_id = 0;
                audio_state.active_sources--;
            }
        }
    }
}

// ===== AUDIO FILE LOADING =====

static audio_format_t detect_audio_format(const char *filename) {
    if (!filename) return AUDIO_FORMAT_UNKNOWN;

    const char *ext = strrchr(filename, '.');
    if (!ext) return AUDIO_FORMAT_UNKNOWN;

    if (strcasecmp(ext, ".wav") == 0) return AUDIO_FORMAT_WAV_CUSTOM;
    if (strcasecmp(ext, ".ogg") == 0) return AUDIO_FORMAT_OGG_CUSTOM;
    if (strcasecmp(ext, ".mp3") == 0) return AUDIO_FORMAT_MP3_CUSTOM;

    return AUDIO_FORMAT_UNKNOWN;
}

static int load_wav_file(const char *filename, ALuint buffer) {
    // Simple WAV file loader
    // In a real implementation, this would parse the WAV header properly

    FILE *file = fopen(filename, "rb");
    if (!file) {
        l_error("Failed to open WAV file: %s", filename);
        return 0;
    }

    // Skip WAV header (simplified)
    fseek(file, 44, SEEK_SET);

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file) - 44;
    fseek(file, 44, SEEK_SET);

    // Allocate buffer
    char *data = malloc(file_size);
    if (!data) {
        fclose(file);
        l_error("Failed to allocate memory for WAV file");
        return 0;
    }

    // Read data
    size_t bytes_read = fread(data, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        free(data);
        l_error("Failed to read WAV file completely");
        return 0;
    }

    // Load into OpenAL buffer
    alBufferData(buffer, AUDIO_FORMAT, data, file_size, AUDIO_SAMPLE_RATE);

    free(data);

    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        l_error("OpenAL error loading WAV: 0x%x", error);
        return 0;
    }

    l_debug("Loaded WAV file: %s (%ld bytes)", filename, file_size);
    return 1;
}

static int load_ogg_file(const char *filename __attribute__((unused)), ALuint buffer __attribute__((unused))) {
    // OGG file loading would require libvorbis integration
    // For now, return failure
    l_error("OGG format not yet implemented: %s", filename);
    return 0;
}

// ===== VOLUME CONTROL =====

void audio_set_master_volume(float volume) {
    if (!audio_state.initialized) return;

    audio_state.master_volume = fmaxf(0.0f, fminf(1.0f, volume));
    alListenerf(AL_GAIN, audio_state.master_volume);

    l_info("Master volume set to %.2f", audio_state.master_volume);
}

void audio_set_music_volume(float volume) {
    if (!audio_state.initialized) return;

    audio_state.music_volume = fmaxf(0.0f, fminf(1.0f, volume));
    l_info("Music volume set to %.2f", audio_state.music_volume);
}

void audio_set_sfx_volume(float volume) {
    if (!audio_state.initialized) return;

    audio_state.sfx_volume = fmaxf(0.0f, fminf(1.0f, volume));
    l_info("SFX volume set to %.2f", audio_state.sfx_volume);
}

float audio_get_master_volume(void) {
    return audio_state.master_volume;
}

float audio_get_music_volume(void) {
    return audio_state.music_volume;
}

float audio_get_sfx_volume(void) {
    return audio_state.sfx_volume;
}

// ===== AUDIO THREAD =====

static int audio_thread_func(SceSize args __attribute__((unused)), void *argp __attribute__((unused))) {
    l_info("Audio thread started");

    while (audio_state.audio_thread_running) {
        if (audio_state.initialized) {
            // Clean up completed sources
            cleanup_completed_sources();

            // Update audio streams
            update_audio_streams();
        }

        // Sleep for 16ms (60 FPS)
        sceKernelDelayThread(16666);
    }

    l_info("Audio thread stopped");
    return 0;
}

static void update_audio_streams(void) {
    // Update streaming audio sources
    for (int i = 0; i < 4; i++) {
        if (audio_state.streams[i].active) {
            // TODO: Implement streaming audio update
            // This would handle buffering of large audio files
        }
    }
}

// ===== UTILITY FUNCTIONS =====

static void create_directories(void) {
    sceIoMkdir("ux0:data/fluffydiver", 0777);
    sceIoMkdir("ux0:data/fluffydiver/audio", 0777);
    sceIoMkdir("ux0:data/fluffydiver/music", 0777);
    sceIoMkdir("ux0:data/fluffydiver/sfx", 0777);
}

int audio_is_playing(int sound_id) {
    if (!audio_state.initialized || sound_id <= 0) {
        return 0;
    }

    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (audio_state.sources[i].active && audio_state.sources[i].sound_id == sound_id) {
            ALint state;
            alGetSourcei(audio_state.sources[i].source, AL_SOURCE_STATE, &state);
            return (state == AL_PLAYING);
        }
    }

    return 0;
}

int audio_get_active_sources(void) {
    return audio_state.active_sources;
}

// ===== CONFIGURATION =====

void audio_enable(int enabled) {
    audio_state.audio_enabled = enabled;
    if (!enabled) {
        audio_stop_all_sounds();
    }
    l_info("Audio %s", enabled ? "enabled" : "disabled");
}

void audio_enable_music(int enabled) {
    audio_state.music_enabled = enabled;
    l_info("Music %s", enabled ? "enabled" : "disabled");
}

void audio_enable_sfx(int enabled) {
    audio_state.sfx_enabled = enabled;
    l_info("SFX %s", enabled ? "enabled" : "disabled");
}

int audio_is_enabled(void) {
    return audio_state.audio_enabled;
}

int audio_is_music_enabled(void) {
    return audio_state.music_enabled;
}

int audio_is_sfx_enabled(void) {
    return audio_state.sfx_enabled;
}

// ===== CLEANUP =====

void audio_cleanup(void) {
    if (!audio_state.initialized) {
        return;
    }

    l_info("Cleaning up audio system");

    // Stop audio thread
    audio_state.audio_thread_running = 0;
    if (audio_state.audio_thread >= 0) {
        sceKernelWaitThreadEnd(audio_state.audio_thread, NULL, NULL);
        sceKernelDeleteThread(audio_state.audio_thread);
    }

    // Stop all sounds
    audio_stop_all_sounds();

    // Clean up sources
    alDeleteSources(MAX_AUDIO_SOURCES, audio_state.source_pool);

    // Clean up buffers
    alDeleteBuffers(MAX_AUDIO_BUFFERS, audio_state.buffer_pool);

    // Clean up OpenAL context
    alcMakeContextCurrent(NULL);
    if (audio_state.context) {
        alcDestroyContext(audio_state.context);
    }
    if (audio_state.device) {
        alcCloseDevice(audio_state.device);
    }

    audio_state.initialized = 0;

    l_success("Audio system cleaned up");
}

// ===== DEBUG FUNCTIONS =====

void audio_debug_info(void) {
    if (!audio_state.initialized) {
        l_warn("Audio system not initialized");
        return;
    }

    l_info("=== Audio Debug Info ===");
    l_info("  Initialized: %s", audio_state.initialized ? "Yes" : "No");
    l_info("  Device: %s", alcGetString(audio_state.device, ALC_DEVICE_SPECIFIER));
    l_info("  Master Volume: %.2f", audio_state.master_volume);
    l_info("  Music Volume: %.2f", audio_state.music_volume);
    l_info("  SFX Volume: %.2f", audio_state.sfx_volume);
    l_info("  Audio Enabled: %s", audio_state.audio_enabled ? "Yes" : "No");
    l_info("  Music Enabled: %s", audio_state.music_enabled ? "Yes" : "No");
    l_info("  SFX Enabled: %s", audio_state.sfx_enabled ? "Yes" : "No");
    l_info("  Active Sources: %d/%d", audio_state.active_sources, MAX_AUDIO_SOURCES);
    l_info("  Next Sound ID: %d", audio_state.next_sound_id);

    // Show active sources
    if (audio_state.active_sources > 0) {
        l_info("  Active Sources:");
        for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
            if (audio_state.sources[i].active) {
                ALint state;
                alGetSourcei(audio_state.sources[i].source, AL_SOURCE_STATE, &state);
                const char *state_str = (state == AL_PLAYING) ? "Playing" :
                (state == AL_PAUSED) ? "Paused" : "Stopped";
                l_info("    [%d] ID:%d %s Vol:%.2f %s", i, audio_state.sources[i].sound_id,
                       state_str, audio_state.sources[i].volume, audio_state.sources[i].filename);
            }
        }
    }
}

// ===== JNI INTEGRATION =====

// Called from JNI when a sound completes
void audio_mark_complete(int sound_id) {
    l_debug("Audio complete callback for sound ID: %d", sound_id);

    // Find the source and mark it as completed
    for (int i = 0; i < MAX_AUDIO_SOURCES; i++) {
        if (audio_state.sources[i].sound_id == sound_id) {
            audio_state.sources[i].active = 0;
            audio_state.sources[i].playing = 0;
            audio_state.sources[i].sound_id = 0;
            audio_state.active_sources--;
            break;
        }
    }
}
