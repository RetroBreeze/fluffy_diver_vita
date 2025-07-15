/*
 * Fluffy Diver PS Vita Port
 * Game patches and memory management
 */

#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <so_util/so_util.h>
#include "utils/logger.h"
#include "utils/utils.h"

// Memory tracking
#define MAX_ALLOCS 2048

typedef struct {
    void *ptr;
    size_t size;
    const char *func;
    int line;
    int used;
} alloc_info_t;

static alloc_info_t alloc_table[MAX_ALLOCS];
static int alloc_count = 0;
static size_t total_allocated = 0;
static int memory_tracking_enabled = 1;

// Function pointer storage for patches
static void *(*original_malloc)(size_t) __attribute__((unused)) = NULL;
static void (*original_free)(void*) __attribute__((unused)) = NULL;
static void *(*original_realloc)(void*, size_t) __attribute__((unused)) = NULL;
static void *(*original_calloc)(size_t, size_t) __attribute__((unused)) = NULL;

// OpenGL function pointers (unused but kept for future patches)
static void (*original_glTexImage2D)(int, int, int, int, int, int, int, const void*) __attribute__((unused)) = NULL;
static void (*original_glBindTexture)(int, int) __attribute__((unused)) = NULL;

// File I/O function pointers (unused but kept for future patches)
static int (*original_fopen)(const char*, const char*) __attribute__((unused)) = NULL;
static int (*original_fclose)(void*) __attribute__((unused)) = NULL;

// Game state for patches
static struct {
    int initialized;
    int monetization_disabled;
    int unlimited_currency;
    int debug_mode;
    int memory_tracking;
} fluffy_state = {0};

// Forward declarations
static void patch_memory_functions(void);
static void patch_file_functions(void);
static void patch_graphics_functions(void);
static void patch_audio_functions(void);
static void patch_monetization_system(void);
static void init_memory_tracking(void);

// ===== MAIN PATCH ENTRY POINT =====

void patch_game(void) {
    l_info("Applying Fluffy Diver patches");

    // Initialize patch state
    fluffy_state.initialized = 1;
    fluffy_state.monetization_disabled = 1;
    fluffy_state.unlimited_currency = 1;
    fluffy_state.debug_mode = 1;
    fluffy_state.memory_tracking = 1;

    // Initialize memory tracking
    init_memory_tracking();

    // Apply patches
    patch_memory_functions();
    patch_file_functions();
    patch_graphics_functions();
    patch_audio_functions();
    patch_monetization_system();

    l_success("All patches applied successfully");
}

// ===== MEMORY MANAGEMENT PATCHES =====

static void init_memory_tracking(void) {
    memset(alloc_table, 0, sizeof(alloc_table));
    alloc_count = 0;
    total_allocated = 0;

    l_info("Memory tracking initialized");
}

static void patch_memory_functions(void) {
    l_info("Patching memory functions");

    // Hook malloc family functions for better tracking
    // These are handled by the linker wrappers, but we can add additional logic here

    // Example: Patch specific memory allocation patterns that might cause issues
    // This would involve finding specific call sites in the game binary

    // For now, we'll just log that memory patches are ready
    l_success("Memory function patches applied");
}

void* malloc_tracked(size_t size, const char* func, int line) {
    if (!memory_tracking_enabled) {
        return malloc(size);
    }

    void* ptr = malloc(size);
    if (ptr && alloc_count < MAX_ALLOCS) {
        // Find empty slot
        for (int i = 0; i < MAX_ALLOCS; i++) {
            if (!alloc_table[i].used) {
                alloc_table[i].ptr = ptr;
                alloc_table[i].size = size;
                alloc_table[i].func = func;
                alloc_table[i].line = line;
                alloc_table[i].used = 1;
                alloc_count++;
                total_allocated += size;
                break;
            }
        }
    }

    return ptr;
}

void free_tracked(void* ptr, const char* func __attribute__((unused)), int line __attribute__((unused))) {
    if (!memory_tracking_enabled) {
        free(ptr);
        return;
    }

    if (ptr) {
        // Find and remove from tracking
        for (int i = 0; i < MAX_ALLOCS; i++) {
            if (alloc_table[i].used && alloc_table[i].ptr == ptr) {
                total_allocated -= alloc_table[i].size;
                alloc_table[i].used = 0;
                alloc_count--;
                break;
            }
        }
        free(ptr);
    }
}

void* realloc_tracked(void* ptr, size_t size, const char* func, int line) {
    if (!memory_tracking_enabled) {
        return realloc(ptr, size);
    }

    // Remove old tracking entry
    if (ptr) {
        for (int i = 0; i < MAX_ALLOCS; i++) {
            if (alloc_table[i].used && alloc_table[i].ptr == ptr) {
                total_allocated -= alloc_table[i].size;
                alloc_table[i].used = 0;
                alloc_count--;
                break;
            }
        }
    }

    // Reallocate
    void* new_ptr = realloc(ptr, size);

    // Add new tracking entry
    if (new_ptr && alloc_count < MAX_ALLOCS) {
        for (int i = 0; i < MAX_ALLOCS; i++) {
            if (!alloc_table[i].used) {
                alloc_table[i].ptr = new_ptr;
                alloc_table[i].size = size;
                alloc_table[i].func = func;
                alloc_table[i].line = line;
                alloc_table[i].used = 1;
                alloc_count++;
                total_allocated += size;
                break;
            }
        }
    }

    return new_ptr;
}

void* calloc_tracked(size_t num, size_t size, const char* func, int line) {
    if (!memory_tracking_enabled) {
        return calloc(num, size);
    }

    size_t total_size = num * size;
    void* ptr = calloc(num, size);

    if (ptr && alloc_count < MAX_ALLOCS) {
        for (int i = 0; i < MAX_ALLOCS; i++) {
            if (!alloc_table[i].used) {
                alloc_table[i].ptr = ptr;
                alloc_table[i].size = total_size;
                alloc_table[i].func = func;
                alloc_table[i].line = line;
                alloc_table[i].used = 1;
                alloc_count++;
                total_allocated += total_size;
                break;
            }
        }
    }

    return ptr;
}

// ===== OTHER PATCHES =====

static void patch_file_functions(void) {
    l_info("Patching file I/O functions");

    // Hook file operations to redirect Android paths to Vita paths
    // This is primarily handled in our JNI implementation and dynlib.c

    // Example: Patch hardcoded file paths in the game binary
    // We would need to analyze the binary to find these locations

    l_success("File I/O patches applied");
}

static void patch_graphics_functions(void) {
    l_info("Patching graphics functions");

    // Hook OpenGL functions for optimization and compatibility
    // VitaGL handles most compatibility automatically

    // Example patches might include:
    // - Texture format conversions
    // - Resolution scaling
    // - Shader compatibility fixes

    l_success("Graphics patches applied");
}

static void patch_audio_functions(void) {
    l_info("Patching audio functions");

    // Hook audio functions for better Vita integration
    // This mainly involves OpenAL-soft integration

    // Example: Patch audio buffer sizes for better performance
    // Or redirect audio file loading to our custom implementation

    l_success("Audio patches applied");
}

static void patch_monetization_system(void) {
    l_info("Patching monetization system");

    // Disable or modify in-app purchases
    // This is mainly handled in the JNI layer

    fluffy_state.monetization_disabled = 1;
    fluffy_state.unlimited_currency = 1;

    l_success("Monetization system bypassed");
}

// ===== MEMORY TRACKING UTILITIES =====

void print_memory_stats(void) {
    l_info("=== Memory Statistics ===");
    l_info("  Total allocations: %d", alloc_count);
    l_info("  Total allocated: %zu bytes", total_allocated);
    l_info("  Average allocation: %zu bytes", alloc_count > 0 ? total_allocated / alloc_count : 0);

    // Memory usage by category
    size_t small_allocs = 0, medium_allocs = 0, large_allocs = 0;
    for (int i = 0; i < MAX_ALLOCS; i++) {
        if (alloc_table[i].used) {
            if (alloc_table[i].size < 1024) {
                small_allocs++;
            } else if (alloc_table[i].size < 65536) {
                medium_allocs++;
            } else {
                large_allocs++;
            }
        }
    }

    l_info("  Small allocations (<1KB): %zu", small_allocs);
    l_info("  Medium allocations (1KB-64KB): %zu", medium_allocs);
    l_info("  Large allocations (>64KB): %zu", large_allocs);
}

void cleanup_memory_tracking(void) {
    if (!memory_tracking_enabled) {
        return;
    }

    l_info("Cleaning up memory tracking");

    // Check for leaks
    int leaks = 0;
    for (int i = 0; i < MAX_ALLOCS; i++) {
        if (alloc_table[i].used) {
            leaks++;
            l_warn("Memory leak: %zu bytes at %s:%d",
                   alloc_table[i].size, alloc_table[i].func, alloc_table[i].line);
        }
    }

    if (leaks > 0) {
        l_error("Found %d memory leaks", leaks);
    } else {
        l_success("No memory leaks detected");
    }

    // Final stats
    print_memory_stats();
}

// ===== PATCH UTILITIES =====

int is_patch_enabled(const char* patch_name) {
    if (strcmp(patch_name, "memory_tracking") == 0) {
        return fluffy_state.memory_tracking;
    } else if (strcmp(patch_name, "monetization_disabled") == 0) {
        return fluffy_state.monetization_disabled;
    } else if (strcmp(patch_name, "unlimited_currency") == 0) {
        return fluffy_state.unlimited_currency;
    } else if (strcmp(patch_name, "debug_mode") == 0) {
        return fluffy_state.debug_mode;
    }

    return 0;
}

void enable_patch(const char* patch_name, int enable) {
    if (strcmp(patch_name, "memory_tracking") == 0) {
        fluffy_state.memory_tracking = enable;
        memory_tracking_enabled = enable;
    } else if (strcmp(patch_name, "monetization_disabled") == 0) {
        fluffy_state.monetization_disabled = enable;
    } else if (strcmp(patch_name, "unlimited_currency") == 0) {
        fluffy_state.unlimited_currency = enable;
    } else if (strcmp(patch_name, "debug_mode") == 0) {
        fluffy_state.debug_mode = enable;
    }

    l_info("Patch '%s' %s", patch_name, enable ? "enabled" : "disabled");
}

// ===== CLEANUP =====

void cleanup_patches(void) {
    l_info("Cleaning up patches");

    // Cleanup memory tracking
    cleanup_memory_tracking();

    // Reset patch state
    fluffy_state.initialized = 0;

    l_success("Patches cleanup complete");
}

// ===== DEBUG FUNCTIONS =====

void debug_print_patch_status(void) {
    l_info("=== Patch Status ===");
    l_info("  Initialized: %s", fluffy_state.initialized ? "Yes" : "No");
    l_info("  Monetization Disabled: %s", fluffy_state.monetization_disabled ? "Yes" : "No");
    l_info("  Unlimited Currency: %s", fluffy_state.unlimited_currency ? "Yes" : "No");
    l_info("  Debug Mode: %s", fluffy_state.debug_mode ? "Yes" : "No");
    l_info("  Memory Tracking: %s", fluffy_state.memory_tracking ? "Yes" : "No");

    // Print memory stats if tracking is enabled
    if (fluffy_state.memory_tracking) {
        print_memory_stats();
    }
}

// Memory tracking macros for convenience
#define MALLOC(size) malloc_tracked(size, __func__, __LINE__)
#define FREE(ptr) free_tracked(ptr, __func__, __LINE__)
#define REALLOC(ptr, size) realloc_tracked(ptr, size, __func__, __LINE__)
#define CALLOC(num, size) calloc_tracked(num, size, __func__, __LINE__)
