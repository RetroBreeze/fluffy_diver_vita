/*
 * Fluffy Diver PS Vita Port
 * Phase 2: Graphics System Implementation
 *
 * Handles VitaGL initialization, OpenGL ES 1.x/2.x dual support,
 * shader management, and graphics optimization for Fluffy Diver
 */

#include <vitaGL.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/gxm.h>
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "utils/logger.h"
#include "utils/utils.h"

// Graphics configuration
#define MEMORY_VITAGL_THRESHOLD_MB 128
#define VITA_SCREEN_WIDTH 960
#define VITA_SCREEN_HEIGHT 544
#define GAME_RENDER_WIDTH 960
#define GAME_RENDER_HEIGHT 544

// Shader cache configuration
#define SHADER_CACHE_SIZE 128
#define SHADER_CACHE_PATH "ux0:data/fluffydiver/shaders"

// Graphics state management
typedef struct {
    int initialized;
    int gl_es_version;
    int screen_width;
    int screen_height;
    int game_width;
    int game_height;
    float scale_x;
    float scale_y;
    int vsync_enabled;
    int antialiasing;
    int texture_filtering;
    int fps_limit;

    // OpenGL context info
    GLint max_texture_size;
    GLint max_texture_units;
    GLint max_vertex_attribs;
    GLint max_fragment_uniform_vectors;
    GLint max_vertex_uniform_vectors;

    // Performance metrics
    int frame_count;
    int last_fps;
    uint64_t last_time;

    // Shader cache
    struct {
        GLuint shader_id;
        char hash[64];
        int used;
    } shader_cache[SHADER_CACHE_SIZE];

} graphics_state_t;

static graphics_state_t graphics_state = {0};

// Forward declarations
static void initialize_vitagl(void);
static void setup_opengl_state(void);
static void probe_gl_capabilities(void);
static void setup_shader_cache(void);
static void update_performance_metrics(void);
static int get_cached_shader(const char *hash);
static void cache_shader(GLuint shader, const char *hash);
static void create_directories(void);

// ===== INITIALIZATION FUNCTIONS =====

int graphics_init(void) {
    l_info("Initializing graphics system for Fluffy Diver");

    // Create required directories
    create_directories();

    // Initialize VitaGL with extended configuration
    initialize_vitagl();

    // Set up OpenGL state
    setup_opengl_state();

    // Probe OpenGL capabilities
    probe_gl_capabilities();

    // Initialize shader cache
    setup_shader_cache();

    // Set up graphics state
    graphics_state.initialized = 1;
    graphics_state.screen_width = VITA_SCREEN_WIDTH;
    graphics_state.screen_height = VITA_SCREEN_HEIGHT;
    graphics_state.game_width = GAME_RENDER_WIDTH;
    graphics_state.game_height = GAME_RENDER_HEIGHT;
    graphics_state.scale_x = (float)VITA_SCREEN_WIDTH / GAME_RENDER_WIDTH;
    graphics_state.scale_y = (float)VITA_SCREEN_HEIGHT / GAME_RENDER_HEIGHT;
    graphics_state.vsync_enabled = 1;
    graphics_state.antialiasing = 1;
    graphics_state.texture_filtering = 1;
    graphics_state.fps_limit = 60;

    // Performance tracking
    graphics_state.frame_count = 0;
    graphics_state.last_fps = 0;
    graphics_state.last_time = sceKernelGetProcessTimeWide();

    l_success("Graphics system initialized successfully");
    l_info("  Screen: %dx%d", graphics_state.screen_width, graphics_state.screen_height);
    l_info("  Game Resolution: %dx%d", graphics_state.game_width, graphics_state.game_height);
    l_info("  Scale: %.2fx%.2f", graphics_state.scale_x, graphics_state.scale_y);
    l_info("  OpenGL ES Version: %d.x", graphics_state.gl_es_version);
    l_info("  Max Texture Size: %d", graphics_state.max_texture_size);
    l_info("  Max Texture Units: %d", graphics_state.max_texture_units);

    return 1;
}

static void initialize_vitagl(void) {
    l_info("Initializing VitaGL with extended configuration");

    // Calculate memory allocation
    int available_memory = sceKernelGetFreeMemorySize(SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW);
    int vitagl_memory = available_memory / 4; // Use 1/4 of available memory for VitaGL

    if (vitagl_memory < 4 * 1024 * 1024) {
        vitagl_memory = 4 * 1024 * 1024; // Minimum 4MB
    }

    l_info("  Available Memory: %d MB", available_memory / (1024 * 1024));
    l_info("  VitaGL Memory: %d MB", vitagl_memory / (1024 * 1024));

    // Initialize VitaGL with extended configuration for Fluffy Diver
    vglInitExtended(
        vitagl_memory,                    // Memory pool size
        VITA_SCREEN_WIDTH,                // Screen width
        VITA_SCREEN_HEIGHT,               // Screen height
        0x1800000,                        // Vertex RAM size (24MB)
    SCE_GXM_MULTISAMPLE_4X           // Anti-aliasing
    );

    // Enable advanced features
    vglUseVram(GL_TRUE);                 // Use VRAM for textures
    vglUseExtraMem(GL_TRUE);             // Use extended memory
    vglEnableRuntimeShaderCompiler(GL_TRUE);  // Enable runtime shader compilation

    l_success("VitaGL initialized successfully");
}

static void setup_opengl_state(void) {
    l_info("Setting up OpenGL state");

    // Set viewport to match screen
    glViewport(0, 0, VITA_SCREEN_WIDTH, VITA_SCREEN_HEIGHT);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set clear color (black)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Enable culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Set up texture filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Check for errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        l_error("OpenGL error during setup: 0x%x", error);
    }

    l_success("OpenGL state configured");
}

static void probe_gl_capabilities(void) {
    l_info("Probing OpenGL capabilities");

    // Get OpenGL version
    const GLubyte *version = glGetString(GL_VERSION);
    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *vendor = glGetString(GL_VENDOR);

    l_info("  OpenGL Version: %s", version);
    l_info("  Renderer: %s", renderer);
    l_info("  Vendor: %s", vendor);

    // Determine OpenGL ES version
    if (strstr((char*)version, "OpenGL ES 2.")) {
        graphics_state.gl_es_version = 2;
    } else if (strstr((char*)version, "OpenGL ES 1.")) {
        graphics_state.gl_es_version = 1;
    } else {
        graphics_state.gl_es_version = 2; // Default to ES 2.0
    }

    // Get implementation limits
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &graphics_state.max_texture_size);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &graphics_state.max_texture_units);

    if (graphics_state.gl_es_version >= 2) {
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &graphics_state.max_vertex_attribs);
        glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &graphics_state.max_fragment_uniform_vectors);
        glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &graphics_state.max_vertex_uniform_vectors);
    }

    l_info("  Max Texture Size: %d", graphics_state.max_texture_size);
    l_info("  Max Texture Units: %d", graphics_state.max_texture_units);

    if (graphics_state.gl_es_version >= 2) {
        l_info("  Max Vertex Attributes: %d", graphics_state.max_vertex_attribs);
        l_info("  Max Fragment Uniform Vectors: %d", graphics_state.max_fragment_uniform_vectors);
        l_info("  Max Vertex Uniform Vectors: %d", graphics_state.max_vertex_uniform_vectors);
    }
}

static void setup_shader_cache(void) {
    l_info("Setting up shader cache");

    // Create shader cache directory
    sceIoMkdir("ux0:data/fluffydiver", 0777);
    sceIoMkdir("ux0:data/fluffydiver/shaders", 0777);

    // Initialize cache
    memset(graphics_state.shader_cache, 0, sizeof(graphics_state.shader_cache));

    l_success("Shader cache initialized");
}

// ===== FRAME MANAGEMENT =====

void graphics_frame_start(void) {
    if (!graphics_state.initialized) {
        return;
    }

    // Clear buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Update performance metrics
    update_performance_metrics();
}

void graphics_frame_end(void) {
    if (!graphics_state.initialized) {
        return;
    }

    // Present frame
    vglSwapBuffers(graphics_state.vsync_enabled ? GL_TRUE : GL_FALSE);

    // Increment frame counter
    graphics_state.frame_count++;
}

// ===== SHADER MANAGEMENT =====

GLuint graphics_load_shader(GLenum type, const char *source) {
    if (!graphics_state.initialized) {
        return 0;
    }

    // Create shader hash for caching
    char hash[64];
    snprintf(hash, sizeof(hash), "%08x_%d", (unsigned int)((uintptr_t)source % 0xFFFFFFFF), type);

    // Check cache first
    int cached_shader = get_cached_shader(hash);
    if (cached_shader > 0) {
        l_debug("Using cached shader: %s", hash);
        return cached_shader;
    }

    // Create new shader
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        l_error("Failed to create shader");
        return 0;
    }

    // Set shader source
    glShaderSource(shader, 1, &source, NULL);

    // Compile shader
    glCompileShader(shader);

    // Check compilation status
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        GLint infoLen;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

        if (infoLen > 1) {
            char *infoLog = malloc(infoLen);
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            l_error("Shader compilation error: %s", infoLog);
            free(infoLog);
        }

        glDeleteShader(shader);
        return 0;
    }

    // Cache the shader
    cache_shader(shader, hash);

    l_debug("Compiled shader: %s", hash);
    return shader;
}

GLuint graphics_create_program(GLuint vertex_shader, GLuint fragment_shader) {
    if (!graphics_state.initialized) {
        return 0;
    }

    // Create program
    GLuint program = glCreateProgram();
    if (program == 0) {
        l_error("Failed to create program");
        return 0;
    }

    // Attach shaders
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);

    // Link program
    glLinkProgram(program);

    // Check link status
    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if (!linked) {
        GLint infoLen;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);

        if (infoLen > 1) {
            char *infoLog = malloc(infoLen);
            glGetProgramInfoLog(program, infoLen, NULL, infoLog);
            l_error("Program link error: %s", infoLog);
            free(infoLog);
        }

        glDeleteProgram(program);
        return 0;
    }

    l_debug("Created program: %d", program);
    return program;
}

// ===== TEXTURE MANAGEMENT =====

GLuint graphics_load_texture(const char *path __attribute__((unused))) {
    if (!graphics_state.initialized) {
        return 0;
    }

    // This is a stub - actual implementation would load texture from file
    // In the real implementation, this would handle various formats:
    // - PNG files from assets
    // - Compressed textures (.pvr, .dds)
    // - Game-specific formats from asset_handler.c

    l_debug("Loading texture: %s", path);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Set default texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // TODO: Load actual texture data from file
    // For now, create a dummy 64x64 white texture
    unsigned char white_data[64 * 64 * 4];
    memset(white_data, 255, sizeof(white_data));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_data);

    l_debug("Created texture: %d", texture);
    return texture;
}

// ===== PERFORMANCE MONITORING =====

static void update_performance_metrics(void) {
    uint64_t current_time = sceKernelGetProcessTimeWide();
    uint64_t delta_time = current_time - graphics_state.last_time;

    // Update FPS every second
    if (delta_time >= 1000000) { // 1 second in microseconds
        graphics_state.last_fps = graphics_state.frame_count;
        graphics_state.frame_count = 0;
        graphics_state.last_time = current_time;

        l_debug("FPS: %d", graphics_state.last_fps);
    }
}

int graphics_get_fps(void) {
    return graphics_state.last_fps;
}

// ===== UTILITY FUNCTIONS =====

static int get_cached_shader(const char *hash) {
    for (int i = 0; i < SHADER_CACHE_SIZE; i++) {
        if (graphics_state.shader_cache[i].used &&
            strcmp(graphics_state.shader_cache[i].hash, hash) == 0) {
            return graphics_state.shader_cache[i].shader_id;
            }
    }
    return 0;
}

static void cache_shader(GLuint shader, const char *hash) {
    for (int i = 0; i < SHADER_CACHE_SIZE; i++) {
        if (!graphics_state.shader_cache[i].used) {
            graphics_state.shader_cache[i].shader_id = shader;
            strncpy(graphics_state.shader_cache[i].hash, hash, sizeof(graphics_state.shader_cache[i].hash) - 1);
            graphics_state.shader_cache[i].used = 1;
            return;
        }
    }

    // Cache full, replace oldest entry (simple FIFO)
    static int next_cache_index = 0;
    int index = next_cache_index % SHADER_CACHE_SIZE;

    if (graphics_state.shader_cache[index].used) {
        glDeleteShader(graphics_state.shader_cache[index].shader_id);
    }

    graphics_state.shader_cache[index].shader_id = shader;
    strncpy(graphics_state.shader_cache[index].hash, hash, sizeof(graphics_state.shader_cache[index].hash) - 1);
    graphics_state.shader_cache[index].hash[sizeof(graphics_state.shader_cache[index].hash) - 1] = '\0';
    graphics_state.shader_cache[index].used = 1;

    next_cache_index++;
}

static void create_directories(void) {
    // Create necessary directories
    sceIoMkdir("ux0:data/fluffydiver", 0777);
    sceIoMkdir("ux0:data/fluffydiver/shaders", 0777);
    sceIoMkdir("ux0:data/fluffydiver/textures", 0777);
}

// ===== COORDINATE TRANSFORMATION =====

void graphics_screen_to_game_coords(float screen_x, float screen_y, float *game_x, float *game_y) {
    if (!graphics_state.initialized) {
        *game_x = screen_x;
        *game_y = screen_y;
        return;
    }

    *game_x = screen_x / graphics_state.scale_x;
    *game_y = screen_y / graphics_state.scale_y;
}

void graphics_game_to_screen_coords(float game_x, float game_y, float *screen_x, float *screen_y) {
    if (!graphics_state.initialized) {
        *screen_x = game_x;
        *screen_y = game_y;
        return;
    }

    *screen_x = game_x * graphics_state.scale_x;
    *screen_y = game_y * graphics_state.scale_y;
}

// ===== CLEANUP =====

void graphics_cleanup(void) {
    if (!graphics_state.initialized) {
        return;
    }

    l_info("Cleaning up graphics system");

    // Clean up shader cache
    for (int i = 0; i < SHADER_CACHE_SIZE; i++) {
        if (graphics_state.shader_cache[i].used) {
            glDeleteShader(graphics_state.shader_cache[i].shader_id);
        }
    }

    // Cleanup VitaGL
    vglEnd();

    graphics_state.initialized = 0;

    l_success("Graphics system cleaned up");
}

// ===== CONFIGURATION =====

void graphics_set_vsync(int enabled) {
    graphics_state.vsync_enabled = enabled;
    l_info("VSync %s", enabled ? "enabled" : "disabled");
}

void graphics_set_fps_limit(int fps) {
    graphics_state.fps_limit = fps;
    l_info("FPS limit set to %d", fps);
}

int graphics_is_initialized(void) {
    return graphics_state.initialized;
}

// ===== OPENGL ES COMPATIBILITY =====

void graphics_enable_gles1_compatibility(void) {
    if (!graphics_state.initialized) {
        return;
    }

    // Enable OpenGL ES 1.x compatibility mode
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    l_info("OpenGL ES 1.x compatibility enabled");
}

void graphics_enable_gles2_compatibility(void) {
    if (!graphics_state.initialized) {
        return;
    }

    // OpenGL ES 2.x is the default mode
    // Just ensure proper state
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    l_info("OpenGL ES 2.x compatibility enabled");
}

// ===== DEBUG FUNCTIONS =====

void graphics_debug_info(void) {
    if (!graphics_state.initialized) {
        l_warn("Graphics system not initialized");
        return;
    }

    l_info("=== Graphics Debug Info ===");
    l_info("  Initialized: %s", graphics_state.initialized ? "Yes" : "No");
    l_info("  OpenGL ES Version: %d.x", graphics_state.gl_es_version);
    l_info("  Screen Resolution: %dx%d", graphics_state.screen_width, graphics_state.screen_height);
    l_info("  Game Resolution: %dx%d", graphics_state.game_width, graphics_state.game_height);
    l_info("  Scale Factor: %.2fx%.2f", graphics_state.scale_x, graphics_state.scale_y);
    l_info("  VSync: %s", graphics_state.vsync_enabled ? "Enabled" : "Disabled");
    l_info("  Anti-aliasing: %s", graphics_state.antialiasing ? "Enabled" : "Disabled");
    l_info("  Current FPS: %d", graphics_state.last_fps);
    l_info("  Frame Count: %d", graphics_state.frame_count);
    l_info("  Max Texture Size: %d", graphics_state.max_texture_size);
    l_info("  Max Texture Units: %d", graphics_state.max_texture_units);

    // Memory information
    SceKernelFreeMemorySizeInfo info;
    sceKernelGetFreeMemorySize(&info);
    l_info("  Free Memory: %d MB", info.size_user / (1024 * 1024));

    // Shader cache status
    int cached_shaders = 0;
    for (int i = 0; i < SHADER_CACHE_SIZE; i++) {
        if (graphics_state.shader_cache[i].used) {
            cached_shaders++;
        }
    }
    l_info("  Cached Shaders: %d/%d", cached_shaders, SHADER_CACHE_SIZE);
}
