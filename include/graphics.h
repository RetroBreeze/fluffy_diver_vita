/*
 * include/graphics.h
 * Graphics System Header for Fluffy Diver PS Vita Port
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <vitaGL.h>

// Graphics initialization and cleanup
int graphics_init(void);
void graphics_cleanup(void);

// Frame management
void graphics_frame_start(void);
void graphics_frame_end(void);

// Shader management
GLuint graphics_load_shader(GLenum type, const char *source);
GLuint graphics_create_program(GLuint vertex_shader, GLuint fragment_shader);

// Texture management
GLuint graphics_load_texture(const char *path);

// Performance monitoring
int graphics_get_fps(void);

// Coordinate transformation
void graphics_screen_to_game_coords(float screen_x, float screen_y, float *game_x, float *game_y);
void graphics_game_to_screen_coords(float game_x, float game_y, float *screen_x, float *screen_y);

// Configuration
void graphics_set_vsync(int enabled);
void graphics_set_fps_limit(int fps);
int graphics_is_initialized(void);

// OpenGL ES compatibility
void graphics_enable_gles1_compatibility(void);
void graphics_enable_gles2_compatibility(void);

// Debug functions
void graphics_debug_info(void);

#endif // GRAPHICS_H
