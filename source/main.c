/*
 * Fluffy Diver PS Vita Port
 * Phase 2: Minimal Working Main
 */

#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/apputil.h>
#include <psp2/system_param.h>

#include <kubridge.h>
#include <so_util/so_util.h>
#include <falso_jni/FalsoJNI.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "utils/logger.h"
#include "utils/init.h"
#include "utils/settings.h"
#include "utils/utils.h"
#include "utils/glutil.h"
#include "graphics.h"
#include "audio.h"

// Game configuration
#define LOAD_ADDRESS 0x98000000
#define LIBRARY_PATH "ux0:data/fluffydiver/libFluffyDiver.so"

// Game state
typedef struct {
    int initialized;
    int running;
    int paused;
    int game_initialized;
    int graphics_ready;
    int audio_ready;

    // Game dimensions
    int screen_width;
    int screen_height;
    int game_width;
    int game_height;

    // Input state
    SceCtrlData ctrl_data;
    SceCtrlData prev_ctrl_data;
    SceTouchData touch_data;
    SceTouchData prev_touch_data;

    // Performance
    uint64_t frame_time;
    uint64_t last_frame_time;
    int fps_counter;
    int target_fps;

    // JNI environment
    JavaVM *java_vm;
    JNIEnv *jni_env;

} fluffy_diver_state_t;

static fluffy_diver_state_t game_state = {0};
static so_module so_mod = {0};

// Forward declarations
static int initialize_systems(void);
static int load_game_library(void);
static int initialize_jni(void);
static void setup_file_paths(void);
static void game_loop(void);
static void update_input(void);
static void process_touch_input(void);
static void simulate_android_touch(float x, float y, int action);
static void handle_vita_controls(void);
static void update_game_logic(void);
static void render_frame(void);
static void handle_system_events(void);
static void cleanup_and_exit(void);

// JNI function prototypes (from game library)
typedef void (*OnGameInitialize_t)(JNIEnv *env, jobject obj);
typedef void (*OnGameUpdate_t)(JNIEnv *env, jobject obj, jint deltaTime);
typedef void (*OnGameTouchEvent_t)(JNIEnv *env, jobject obj, jint action, jfloat x, jfloat y);
typedef void (*OnGamePause_t)(JNIEnv *env, jobject obj);
typedef void (*OnGameResume_t)(JNIEnv *env, jobject obj);
typedef void (*OnGameBack_t)(JNIEnv *env, jobject obj);

// Game function pointers
static OnGameInitialize_t game_initialize = NULL;
static OnGameUpdate_t game_update = NULL;
static OnGameTouchEvent_t game_touch_event = NULL;
static OnGamePause_t game_pause = NULL;
static OnGameResume_t game_resume = NULL;
static OnGameBack_t game_back = NULL;

// ===== MAIN ENTRY POINT =====

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    l_info("=== Fluffy Diver PS Vita Port Phase 2 ===");
    l_info("Graphics and Audio Integration Build");
    l_info("Version: 1.0.0");
    l_info("Build Date: " __DATE__ " " __TIME__);

    // Initialize all systems
    if (!initialize_systems()) {
        l_error("Failed to initialize systems");
        cleanup_and_exit();
        return -1;
    }

    // Load game library
    if (!load_game_library()) {
        l_error("Failed to load game library");
        cleanup_and_exit();
        return -1;
    }

    // Initialize JNI environment
    if (!initialize_jni()) {
        l_error("Failed to initialize JNI");
        cleanup_and_exit();
        return -1;
    }

    // Set up file paths
    setup_file_paths();

    // Initialize game
    if (game_initialize) {
        l_info("Initializing game...");
        game_initialize(game_state.jni_env, NULL);
        game_state.game_initialized = 1;
        l_success("Game initialized successfully");
    }

    // Main game loop
    game_state.running = 1;
    l_info("Starting main game loop");
    game_loop();

    // Cleanup and exit
    cleanup_and_exit();
    return 0;
}

// ===== SYSTEM INITIALIZATION =====

static int initialize_systems(void) {
    l_info("Initializing Fluffy Diver systems...");

    // Initialize basic utilities (from boilerplate)

    // Initialize input
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

    // Initialize graphics system
    l_info("Initializing graphics system...");
    if (!graphics_init()) {
        l_error("Failed to initialize graphics");
        return 0;
    }
    game_state.graphics_ready = 1;

    // Initialize audio system
    l_info("Initializing audio system...");
    if (!audio_init()) {
        l_error("Failed to initialize audio");
        return 0;
    }
    game_state.audio_ready = 1;

    // Set up game state
    game_state.initialized = 1;
    game_state.screen_width = 960;
    game_state.screen_height = 544;
    game_state.game_width = 960;
    game_state.game_height = 544;
    game_state.target_fps = 60;
    game_state.last_frame_time = sceKernelGetProcessTimeWide();

    l_success("All systems initialized successfully");
    return 1;
}

static int load_game_library(void) {
    l_info("Loading game library: %s", LIBRARY_PATH);

    // Check if library file exists
    SceUID fd = sceIoOpen(LIBRARY_PATH, SCE_O_RDONLY, 0);
    if (fd < 0) {
        l_error("Game library not found: %s", LIBRARY_PATH);
        return 0;
    }
    sceIoClose(fd);

    // Load the library
    if (so_file_load(&so_mod, LIBRARY_PATH, LOAD_ADDRESS) < 0) {
        l_error("Failed to load game library");
        return 0;
    }

    l_success("Game library loaded successfully");
    l_info("  Text base: 0x%08X", so_mod.text_base);
    l_info("  Text size: 0x%08X", so_mod.text_size);
    l_info("  Data base: 0x%08X", so_mod.data_base);
    l_info("  Data size: 0x%08X", so_mod.data_size);

    // Resolve game functions
    game_initialize = (OnGameInitialize_t)so_symbol(&so_mod, "Java_com_hotdog_jni_Natives_OnGameInitialize");
    game_update = (OnGameUpdate_t)so_symbol(&so_mod, "Java_com_hotdog_jni_Natives_OnGameUpdate");
    game_touch_event = (OnGameTouchEvent_t)so_symbol(&so_mod, "Java_com_hotdog_jni_Natives_OnGameTouchEvent");
    game_pause = (OnGamePause_t)so_symbol(&so_mod, "Java_com_hotdog_jni_Natives_OnGamePause");
    game_resume = (OnGameResume_t)so_symbol(&so_mod, "Java_com_hotdog_jni_Natives_OnGameResume");
    game_back = (OnGameBack_t)so_symbol(&so_mod, "Java_com_hotdog_jni_Natives_OnGameBack");

    l_info("Game function resolution:");
    l_info("  OnGameInitialize: %p", game_initialize);
    l_info("  OnGameUpdate: %p", game_update);
    l_info("  OnGameTouchEvent: %p", game_touch_event);
    l_info("  OnGamePause: %p", game_pause);
    l_info("  OnGameResume: %p", game_resume);
    l_info("  OnGameBack: %p", game_back);

    return 1;
}

static int initialize_jni(void) {
    l_info("Initializing JNI environment");

    // FalsoJNI globals are available automatically
    game_state.java_vm = &jvm;
    game_state.jni_env = &jni;

    l_success("JNI environment initialized");
    return 1;
}

static void setup_file_paths(void) {
    l_info("Setting up file paths");

    // Create required directories
    sceIoMkdir("ux0:data/fluffydiver", 0777);
    sceIoMkdir("ux0:data/fluffydiver/assets", 0777);
    sceIoMkdir("ux0:data/fluffydiver/save", 0777);
    sceIoMkdir("ux0:data/fluffydiver/cache", 0777);

    l_success("File paths configured");
}

// ===== MAIN GAME LOOP =====

static void game_loop(void) {
    l_info("Entering main game loop");

    while (game_state.running) {
        // Calculate frame time
        uint64_t current_time = sceKernelGetProcessTimeWide();
        game_state.frame_time = current_time - game_state.last_frame_time;
        game_state.last_frame_time = current_time;

        // Start frame
        if (game_state.graphics_ready) {
            graphics_frame_start();
        }

        // Handle system events
        handle_system_events();

        // Update input
        update_input();

        // Update game logic
        if (!game_state.paused) {
            update_game_logic();
        }

        // Render frame
        if (game_state.graphics_ready) {
            render_frame();
            graphics_frame_end();
        }

        // FPS limiting
        if (game_state.target_fps > 0) {
            uint64_t target_frame_time = 1000000 / game_state.target_fps;
            uint64_t actual_frame_time = sceKernelGetProcessTimeWide() - current_time;
            if (actual_frame_time < target_frame_time) {
                sceKernelDelayThread(target_frame_time - actual_frame_time);
            }
        }

        game_state.fps_counter++;
    }

    l_info("Exiting main game loop");
}

// ===== INPUT HANDLING =====

static void update_input(void) {
    // Store previous input state
    game_state.prev_ctrl_data = game_state.ctrl_data;
    game_state.prev_touch_data = game_state.touch_data;

    // Read current input
    sceCtrlPeekBufferPositive(0, &game_state.ctrl_data, 1);
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &game_state.touch_data, 1);

    // Process touch input
    process_touch_input();

    // Handle Vita controls
    handle_vita_controls();
}

static void process_touch_input(void) {
    // Process direct touch input
    if (game_state.touch_data.reportNum > 0) {
        for (unsigned int i = 0; i < game_state.touch_data.reportNum; i++) {
            float x = game_state.touch_data.report[i].x;
            float y = game_state.touch_data.report[i].y;

            // Convert touch coordinates to game coordinates
            float game_x, game_y;
            graphics_screen_to_game_coords(x, y, &game_x, &game_y);

            // Check if this is a new touch
            int is_new_touch = 1;
            if (game_state.prev_touch_data.reportNum > 0) {
                for (unsigned int j = 0; j < game_state.prev_touch_data.reportNum; j++) {
                    if (game_state.prev_touch_data.report[j].id == game_state.touch_data.report[i].id) {
                        is_new_touch = 0;
                        break;
                    }
                }
            }

            if (is_new_touch) {
                simulate_android_touch(game_x, game_y, 0); // ACTION_DOWN
            } else {
                simulate_android_touch(game_x, game_y, 2); // ACTION_MOVE
            }
        }
    }

    // Handle touch release
    if (game_state.prev_touch_data.reportNum > 0 && game_state.touch_data.reportNum == 0) {
        // All touches released
        float x = game_state.prev_touch_data.report[0].x;
        float y = game_state.prev_touch_data.report[0].y;
        float game_x, game_y;
        graphics_screen_to_game_coords(x, y, &game_x, &game_y);
        simulate_android_touch(game_x, game_y, 1); // ACTION_UP
    }
}

static void simulate_android_touch(float x, float y, int action) {
    if (game_touch_event && game_state.game_initialized) {
        game_touch_event(game_state.jni_env, NULL, action, x, y);
    }
}

static void handle_vita_controls(void) {
    // Handle button presses
    uint32_t pressed = game_state.ctrl_data.buttons & ~game_state.prev_ctrl_data.buttons;

    // Back button (SELECT)
    if (pressed & SCE_CTRL_SELECT) {
        if (game_back && game_state.game_initialized) {
            game_back(game_state.jni_env, NULL);
        }
    }

    // Start button - pause/resume
    if (pressed & SCE_CTRL_START) {
        if (game_state.paused) {
            game_state.paused = 0;
            if (game_resume && game_state.game_initialized) {
                game_resume(game_state.jni_env, NULL);
            }
        } else {
            game_state.paused = 1;
            if (game_pause && game_state.game_initialized) {
                game_pause(game_state.jni_env, NULL);
            }
        }
    }

    // Map face buttons to touch events
    if (pressed & SCE_CTRL_CROSS) {
        simulate_android_touch(480, 272, 0); // Center screen touch down
    }
    if ((game_state.prev_ctrl_data.buttons & SCE_CTRL_CROSS) && !(game_state.ctrl_data.buttons & SCE_CTRL_CROSS)) {
        simulate_android_touch(480, 272, 1); // Center screen touch up
    }
}

// ===== GAME LOGIC =====

static void update_game_logic(void) {
    if (game_update && game_state.game_initialized) {
        int delta_time = (int)(game_state.frame_time / 1000); // Convert to milliseconds
        game_update(game_state.jni_env, NULL, delta_time);
    }
}

static void render_frame(void) {
    // The actual rendering is handled by the game library
    // We just need to provide the OpenGL context and handle frame presentation

    // Display debug info if needed
    if (game_state.fps_counter % 60 == 0) { // Every second
        l_debug("FPS: %d, Frame time: %llu us", graphics_get_fps(), game_state.frame_time);
    }
}

// ===== SYSTEM EVENTS =====

static void handle_system_events(void) {
    // Handle exit request
    if (game_state.ctrl_data.buttons & SCE_CTRL_LTRIGGER &&
        game_state.ctrl_data.buttons & SCE_CTRL_RTRIGGER &&
        game_state.ctrl_data.buttons & SCE_CTRL_SELECT) {
        // L + R + SELECT = Exit
        l_info("Exit requested by user");
    game_state.running = 0;
        }
}

// ===== CLEANUP =====

static void cleanup_and_exit(void) {
    l_info("Cleaning up and exiting...");

    // Stop game
    if (game_state.game_initialized && game_pause) {
        game_pause(game_state.jni_env, NULL);
    }

    // Cleanup audio system
    if (game_state.audio_ready) {
        audio_cleanup();
    }

    // Cleanup graphics system
    if (game_state.graphics_ready) {
        graphics_cleanup();
    }

    // Cleanup - handled by boilerplate

    l_success("Cleanup complete");

    // Exit
    sceKernelExitProcess(0);
}

// ===== DEBUG FUNCTIONS =====

void debug_print_system_info(void) {
    l_info("=== Fluffy Diver System Info ===");
    l_info("  Game State:");
    l_info("    Initialized: %s", game_state.initialized ? "Yes" : "No");
    l_info("    Running: %s", game_state.running ? "Yes" : "No");
    l_info("    Paused: %s", game_state.paused ? "Yes" : "No");
    l_info("    Game Initialized: %s", game_state.game_initialized ? "Yes" : "No");
    l_info("    Graphics Ready: %s", game_state.graphics_ready ? "Yes" : "No");
    l_info("    Audio Ready: %s", game_state.audio_ready ? "Yes" : "No");
    l_info("  Performance:");
    l_info("    Target FPS: %d", game_state.target_fps);
    l_info("    Frame Time: %llu us", game_state.frame_time);
    l_info("    FPS Counter: %d", game_state.fps_counter);

    // Print graphics debug info
    if (game_state.graphics_ready) {
        graphics_debug_info();
    }

    // Print audio debug info
    if (game_state.audio_ready) {
        audio_debug_info();
    }

    // Memory info
    SceKernelFreeMemorySizeInfo info;
    sceKernelGetFreeMemorySize(&info);
    l_info("  Memory: %d KB free", info.size_user / 1024);
}

// ===== UTILITY FUNCTIONS =====

int is_game_running(void) {
    return game_state.running;
}

int is_game_paused(void) {
    return game_state.paused;
}

void set_game_paused(int paused) {
    game_state.paused = paused;
}

uint64_t get_frame_time(void) {
    return game_state.frame_time;
}

int get_fps(void) {
    return graphics_get_fps();
}
