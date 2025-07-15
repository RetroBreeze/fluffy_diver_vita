/*
 * Fluffy Diver PS Vita Port
 * Main entry point and initialization
 */

#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/settings.h"
#include "utils/utils.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/display.h>
#include <psp2/kernel/clib.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Memory configuration for Fluffy Diver
int _newlib_heap_size_user = 256 * 1024 * 1024;  // 256MB heap

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;  // 4MB for file I/O
#endif

// Global so_module instance
so_module so_mod;

// Game state structure
typedef struct {
    int initialized;
    int screen_width;
    int screen_height;
    int game_running;
    int game_paused;
    float delta_time;
    uint64_t last_frame_time;
} fluffy_diver_state_t;

static fluffy_diver_state_t game_state = {0};

// Forward declarations
void init_fluffy_diver_systems(void);
void update_game_frame(void);
void handle_vita_input(void);
void cleanup_on_exit(void);

// JNI method declarations for Fluffy Diver
extern void Java_com_hotdog_jni_Natives_OnGameInitialize(JNIEnv *env, jobject obj);
extern void Java_com_hotdog_jni_Natives_OnGameUpdate(JNIEnv *env, jobject obj, jint deltaTime);
extern void Java_com_hotdog_jni_Natives_OnGameTouchEvent(JNIEnv *env, jobject obj, jint action, jfloat x, jfloat y);
extern void Java_com_hotdog_jni_Natives_OnGamePause(JNIEnv *env, jobject obj);
extern void Java_com_hotdog_jni_Natives_OnGameResume(JNIEnv *env, jobject obj);
extern void Java_com_hotdog_jni_Natives_OnGameBack(JNIEnv *env, jobject obj);

int main() {
    l_info("Starting Fluffy Diver PS Vita Port");
    l_info("Version: %s", VITA_VERSION);
    l_info("Build: %s %s", __DATE__, __TIME__);

    // Initialize all core systems
    soloader_init_all();
    l_success("Core systems initialized");

    // Initialize Fluffy Diver specific systems
    init_fluffy_diver_systems();
    l_success("Fluffy Diver systems initialized");

    // Load JNI_OnLoad symbol and call it
    int (*JNI_OnLoad)(void *jvm) = (void *)so_symbol(&so_mod, "JNI_OnLoad");
    if (JNI_OnLoad) {
        l_info("Calling JNI_OnLoad");
        JNI_OnLoad(&jvm);
        l_success("JNI_OnLoad completed");
    } else {
        l_warning("JNI_OnLoad not found, continuing without it");
    }

    // Initialize OpenGL context
    gl_init();
    l_success("OpenGL context initialized");

    // Initialize game through JNI
    JNIEnv *env = jni_get_env();
    if (env) {
        l_info("Calling OnGameInitialize");
        Java_com_hotdog_jni_Natives_OnGameInitialize(env, NULL);
        game_state.initialized = 1;
        game_state.game_running = 1;
        l_success("Game initialized successfully");
    } else {
        l_fatal("Failed to get JNI environment");
        fatal_error("JNI environment initialization failed");
    }

    // Main game loop
    l_info("Starting main game loop");
    uint64_t last_time = sceKernelGetProcessTimeWide();

    while (game_state.game_running) {
        uint64_t current_time = sceKernelGetProcessTimeWide();
        game_state.delta_time = (float)(current_time - last_time) / 1000000.0f; // Convert to seconds
        last_time = current_time;

        // Handle input
        handle_vita_input();

        // Update game logic
        update_game_frame();

        // Render frame
        gl_swap();

        // Maintain frame rate (target 60 FPS)
        sceKernelDelayThread(16666); // ~60 FPS
    }

    // Cleanup
    cleanup_on_exit();
    l_info("Fluffy Diver exiting cleanly");

    sceKernelExitDeleteThread(0);
    return 0;
}

void init_fluffy_diver_systems(void) {
    l_info("Initializing Fluffy Diver systems");

    // Set up game state
    game_state.screen_width = 960;
    game_state.screen_height = 544;
    game_state.game_running = 0;
    game_state.game_paused = 0;
    game_state.delta_time = 0.0f;
    game_state.last_frame_time = 0;

    // Initialize controllers
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    l_success("Controller initialized");

    // Initialize touch
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    l_success("Touch initialized");

    // Set optimal power profile for gaming
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
    l_success("Power profile optimized for gaming");

    l_success("Fluffy Diver systems initialization complete");
}

void update_game_frame(void) {
    if (!game_state.initialized || game_state.game_paused) {
        return;
    }

    // Convert delta time to milliseconds for Android game
    jint delta_ms = (jint)(game_state.delta_time * 1000.0f);

    // Call game update through JNI
    JNIEnv *env = jni_get_env();
    if (env) {
        Java_com_hotdog_jni_Natives_OnGameUpdate(env, NULL, delta_ms);
    }
}

void handle_vita_input(void) {
    static SceCtrlData pad_prev = {0};
    SceCtrlData pad;

    // Read controller input
    sceCtrlPeekBufferPositive(0, &pad, 1);

    // Handle back button (SELECT)
    if ((pad.buttons & SCE_CTRL_SELECT) && !(pad_prev.buttons & SCE_CTRL_SELECT)) {
        JNIEnv *env = jni_get_env();
        if (env) {
            Java_com_hotdog_jni_Natives_OnGameBack(env, NULL);
        }
    }

    // Handle start button for pause/resume
    if ((pad.buttons & SCE_CTRL_START) && !(pad_prev.buttons & SCE_CTRL_START)) {
        JNIEnv *env = jni_get_env();
        if (env) {
            if (game_state.game_paused) {
                Java_com_hotdog_jni_Natives_OnGameResume(env, NULL);
                game_state.game_paused = 0;
                l_info("Game resumed");
            } else {
                Java_com_hotdog_jni_Natives_OnGamePause(env, NULL);
                game_state.game_paused = 1;
                l_info("Game paused");
            }
        }
    }

    // Handle touch input (simplified mapping)
    if (pad.buttons & SCE_CTRL_CROSS) {
        // Convert analog stick to touch coordinates
        float touch_x = 480.0f + (pad.lx - 128.0f) * (480.0f / 128.0f);
        float touch_y = 272.0f + (pad.ly - 128.0f) * (272.0f / 128.0f);

        // Clamp to screen bounds
        touch_x = touch_x < 0.0f ? 0.0f : (touch_x > 960.0f ? 960.0f : touch_x);
        touch_y = touch_y < 0.0f ? 0.0f : (touch_y > 544.0f ? 544.0f : touch_y);

        JNIEnv *env = jni_get_env();
        if (env) {
            Java_com_hotdog_jni_Natives_OnGameTouchEvent(env, NULL, 0, touch_x, touch_y);
        }
    }

    // Handle actual touch input
    SceTouchData touch_data;
    if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch_data, 1) > 0) {
        if (touch_data.reportNum > 0) {
            // Convert touch coordinates to screen coordinates
            float touch_x = (touch_data.report[0].x * 960.0f) / 1920.0f;
            float touch_y = (touch_data.report[0].y * 544.0f) / 1088.0f;

            JNIEnv *env = jni_get_env();
            if (env) {
                Java_com_hotdog_jni_Natives_OnGameTouchEvent(env, NULL, 0, touch_x, touch_y);
            }
        }
    }

    pad_prev = pad;
}

void cleanup_on_exit(void) {
    l_info("Cleaning up Fluffy Diver resources");

    // Pause game before exit
    if (game_state.initialized && !game_state.game_paused) {
        JNIEnv *env = jni_get_env();
        if (env) {
            Java_com_hotdog_jni_Natives_OnGamePause(env, NULL);
        }
    }

    // Cleanup OpenGL
    gl_cleanup();

    // Cleanup touch
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_STOP);

    l_success("Cleanup complete");
}
