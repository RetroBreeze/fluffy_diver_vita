/*
 * Fluffy Diver PS Vita Port
 * Configuration header file
 */

#ifndef FLUFFYDIVER_CONFIG_H
#define FLUFFYDIVER_CONFIG_H

// Game information
#define GAME_NAME           "Fluffy Diver"
#define GAME_VERSION        "1.0.0"
#define GAME_TITLE_ID       "FLUF00001"

// Memory configuration
#define LOAD_ADDRESS        0x98000000
#define HEAP_SIZE          (256 * 1024 * 1024)  // 256MB
#define STACK_SIZE         (1024 * 1024)         // 1MB

// File paths
#define SO_PATH            "ux0:data/fluffydiver/libFluffyDiver.so"
#define DATA_PATH          "ux0:data/fluffydiver"
#define ASSETS_PATH        "ux0:data/fluffydiver/assets"
#define SAVE_PATH          "ux0:data/fluffydiver/save"
#define SETTINGS_PATH      "ux0:data/fluffydiver/settings.cfg"

// Screen configuration
#define SCREEN_WIDTH        960
#define SCREEN_HEIGHT       544
#define TARGET_FPS          60

// OpenGL configuration
#define VERTEX_POOL_SIZE    (2 * 1024 * 1024)   // 2MB
#define VERTEX_RAM_SIZE     (24 * 1024 * 1024)  // 24MB
#define TEXTURE_CACHE_SIZE  (64 * 1024 * 1024)  // 64MB

// Audio configuration
#define MAX_AUDIO_SOURCES   32
#define AUDIO_SAMPLE_RATE   44100
#define AUDIO_BUFFER_SIZE   1024

// Input configuration
#define TOUCH_DEADZONE      0.1f
#define ANALOG_DEADZONE     0.2f

// Debug configuration
#ifdef DEBUG
#define DEBUG_MEMORY        1
#define DEBUG_GRAPHICS      1
#define DEBUG_AUDIO         1
#define DEBUG_INPUT         1
#define DEBUG_JNI           1
#define LOG_LEVEL           LOG_DEBUG
#else
#define DEBUG_MEMORY        0
#define DEBUG_GRAPHICS      0
#define DEBUG_AUDIO         0
#define DEBUG_INPUT         0
#define DEBUG_JNI           0
#define LOG_LEVEL           LOG_INFO
#endif

// Performance configuration
#define ENABLE_PROFILING    1
#define ENABLE_THREADING    1
#define ENABLE_VSYNC        1
#define ENABLE_MSAA         1

// Feature toggles
#define ENABLE_SAVE_SYSTEM  1
#define ENABLE_ACHIEVEMENTS 1
#define ENABLE_LEADERBOARDS 0  // Disabled for offline play
#define ENABLE_MONETIZATION 0  // Disabled for Vita port

// JNI configuration
#define JNI_METHOD_COUNT    13
#define JNI_CLASS_COUNT     3

// Game-specific configuration
#define UNLIMITED_CURRENCY  1
#define UNLOCK_ALL_CONTENT  1
#define SKIP_INTRO          0
#define ENABLE_CHEATS       1

// Platform-specific defines
#define PLATFORM_VITA       1
#define PLATFORM_ANDROID    0

// Compiler-specific configuration
#ifdef __GNUC__
#define FORCE_INLINE    __attribute__((always_inline)) inline
#define NO_INLINE       __attribute__((noinline))
#define ALIGNED(x)      __attribute__((aligned(x)))
#define PACKED          __attribute__((packed))
#else
#define FORCE_INLINE    inline
#define NO_INLINE
#define ALIGNED(x)
#define PACKED
#endif

// Utility macros
#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b)          ((a) < (b) ? (a) : (b))
#define MAX(a, b)          ((a) > (b) ? (a) : (b))
#define CLAMP(x, min, max) (MIN(MAX(x, min), max))

// Error codes
#define ERROR_NONE          0
#define ERROR_INIT_FAILED   -1
#define ERROR_LOAD_FAILED   -2
#define ERROR_MEMORY_FAILED -3
#define ERROR_FILE_FAILED   -4
#define ERROR_GRAPHICS_FAILED -5
#define ERROR_AUDIO_FAILED  -6

// Log levels
#define LOG_DEBUG           0
#define LOG_INFO            1
#define LOG_WARNING         2
#define LOG_ERROR           3
#define LOG_FATAL           4

// Touch actions (Android compatibility)
#define TOUCH_ACTION_DOWN   0
#define TOUCH_ACTION_UP     1
#define TOUCH_ACTION_MOVE   2
#define TOUCH_ACTION_CANCEL 3

// Key codes (Android compatibility)
#define KEYCODE_BACK        4
#define KEYCODE_MENU        82
#define KEYCODE_HOME        3

// Audio formats
#define AUDIO_FORMAT_OGG    1
#define AUDIO_FORMAT_WAV    2
#define AUDIO_FORMAT_MP3    3

// Graphics formats
#define TEXTURE_FORMAT_RGBA8888  1
#define TEXTURE_FORMAT_RGB565    2
#define TEXTURE_FORMAT_RGBA4444  3

// Version checking
#define VERSION_MAJOR       1
#define VERSION_MINOR       0
#define VERSION_PATCH       0
#define VERSION_BUILD       1

#define MAKE_VERSION(major, minor, patch) \
((major) << 16 | (minor) << 8 | (patch))

#define CURRENT_VERSION \
MAKE_VERSION(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH)

// Build information
#define BUILD_DATE          __DATE__
#define BUILD_TIME          __TIME__
#define BUILD_COMPILER      __VERSION__

// Safety checks
#if HEAP_SIZE > (400 * 1024 * 1024)
#error "Heap size too large for PS Vita"
#endif

#if VERTEX_RAM_SIZE > (32 * 1024 * 1024)
#error "Vertex RAM size too large"
#endif

#if MAX_AUDIO_SOURCES > 64
#error "Too many audio sources"
#endif

#endif // FLUFFYDIVER_CONFIG_H
