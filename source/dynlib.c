/*
 * Fluffy Diver PS Vita Port
 * Dynamic library symbol resolution
 * Based on analysis of libFluffyDiver.so dependencies
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>
#include <locale.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <pthread.h>

#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/system_param.h>

#include <vitaGL.h>
#include <zlib.h>

#include <so_util/so_util.h>
#include "utils/logger.h"

// Fake FILE structure for compatibility
FILE __sF_fake[3];

// Android logging functions
int __android_log_print(int prio __attribute__((unused)), const char *tag __attribute__((unused)), const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = vprintf(fmt, args);
    va_end(args);
    return result;
}

int __android_log_vprint(int prio __attribute__((unused)), const char *tag __attribute__((unused)), const char *fmt, va_list ap) {
    return vprintf(fmt, ap);
}

void __android_log_assert(const char *cond, const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("ASSERT: %s %s: ", tag, cond);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    abort();
}

// Errno location function
int* __errno_location(void) {
    return &errno;
}

// OpenGL ES stub functions for unsupported functions
void glDetachShader_stub(GLuint program __attribute__((unused)), GLuint shader __attribute__((unused))) {
    // VitaGL doesn't support glDetachShader, so we'll just log it
    l_debug("glDetachShader called (stubbed)");
}

// Network stub functions
int gethostname(char *name, size_t len) {
    strncpy(name, "psvita", len);
    return 0;
}

// Symbol resolution table
so_default_dynlib default_dynlib[] = {
    // Memory functions
    {"malloc", (uintptr_t)&malloc},
    {"free", (uintptr_t)&free},
    {"calloc", (uintptr_t)&calloc},
    {"realloc", (uintptr_t)&realloc},
    {"memcpy", (uintptr_t)&memcpy},
    {"memmove", (uintptr_t)&memmove},
    {"memset", (uintptr_t)&memset},
    {"memcmp", (uintptr_t)&memcmp},
    {"memchr", (uintptr_t)&memchr},

    // String functions
    {"strlen", (uintptr_t)&strlen},
    {"strcpy", (uintptr_t)&strcpy},
    {"strncpy", (uintptr_t)&strncpy},
    {"strcat", (uintptr_t)&strcat},
    {"strncat", (uintptr_t)&strncat},
    {"strcmp", (uintptr_t)&strcmp},
    {"strncmp", (uintptr_t)&strncmp},
    {"strchr", (uintptr_t)&strchr},
    {"strrchr", (uintptr_t)&strrchr},
    {"strstr", (uintptr_t)&strstr},
    {"strdup", (uintptr_t)&strdup},
    {"strcasecmp", (uintptr_t)&strcasecmp},
    {"strncasecmp", (uintptr_t)&strncasecmp},
    {"strtok", (uintptr_t)&strtok},
    {"strtol", (uintptr_t)&strtol},
    {"strtoul", (uintptr_t)&strtoul},
    {"strtod", (uintptr_t)&strtod},
    {"strtof", (uintptr_t)&strtof},
    {"atoi", (uintptr_t)&atoi},
    {"atol", (uintptr_t)&atol},
    {"atof", (uintptr_t)&atof},

    // Math functions
    {"sin", (uintptr_t)&sin},
    {"cos", (uintptr_t)&cos},
    {"tan", (uintptr_t)&tan},
    {"asin", (uintptr_t)&asin},
    {"acos", (uintptr_t)&acos},
    {"atan", (uintptr_t)&atan},
    {"atan2", (uintptr_t)&atan2},
    {"sinh", (uintptr_t)&sinh},
    {"cosh", (uintptr_t)&cosh},
    {"tanh", (uintptr_t)&tanh},
    {"exp", (uintptr_t)&exp},
    {"log", (uintptr_t)&log},
    {"log10", (uintptr_t)&log10},
    {"pow", (uintptr_t)&pow},
    {"sqrt", (uintptr_t)&sqrt},
    {"ceil", (uintptr_t)&ceil},
    {"floor", (uintptr_t)&floor},
    {"fabs", (uintptr_t)&fabs},
    {"fmod", (uintptr_t)&fmod},
    {"sinf", (uintptr_t)&sinf},
    {"cosf", (uintptr_t)&cosf},
    {"tanf", (uintptr_t)&tanf},
    {"asinf", (uintptr_t)&asinf},
    {"acosf", (uintptr_t)&acosf},
    {"atanf", (uintptr_t)&atanf},
    {"atan2f", (uintptr_t)&atan2f},
    {"expf", (uintptr_t)&expf},
    {"logf", (uintptr_t)&logf},
    {"log10f", (uintptr_t)&log10f},
    {"powf", (uintptr_t)&powf},
    {"sqrtf", (uintptr_t)&sqrtf},
    {"floorf", (uintptr_t)&floorf},
    {"ceilf", (uintptr_t)&ceilf},
    {"fabsf", (uintptr_t)&fabsf},
    {"fmodf", (uintptr_t)&fmodf},

    // OpenGL ES 1.x functions
    {"glActiveTexture", (uintptr_t)&glActiveTexture},
    {"glAlphaFunc", (uintptr_t)&glAlphaFunc},
    {"glBindTexture", (uintptr_t)&glBindTexture},
    {"glBlendFunc", (uintptr_t)&glBlendFunc},
    {"glClear", (uintptr_t)&glClear},
    {"glClearColor", (uintptr_t)&glClearColor},
    {"glClearDepthf", (uintptr_t)&glClearDepthf},
    {"glClientActiveTexture", (uintptr_t)&glClientActiveTexture},
    {"glColor4f", (uintptr_t)&glColor4f},
    {"glColorPointer", (uintptr_t)&glColorPointer},
    {"glDeleteTextures", (uintptr_t)&glDeleteTextures},
    {"glDepthFunc", (uintptr_t)&glDepthFunc},
    {"glDepthMask", (uintptr_t)&glDepthMask},
    {"glDisable", (uintptr_t)&glDisable},
    {"glDisableClientState", (uintptr_t)&glDisableClientState},
    {"glDrawArrays", (uintptr_t)&glDrawArrays},
    {"glEnable", (uintptr_t)&glEnable},
    {"glEnableClientState", (uintptr_t)&glEnableClientState},
    {"glFinish", (uintptr_t)&glFinish},
    {"glFlush", (uintptr_t)&glFlush},
    {"glFrustumf", (uintptr_t)&glFrustumf},
    {"glGenTextures", (uintptr_t)&glGenTextures},
    {"glGetError", (uintptr_t)&glGetError},
    {"glGetString", (uintptr_t)&glGetString},
    {"glLoadIdentity", (uintptr_t)&glLoadIdentity},
    {"glLoadMatrixf", (uintptr_t)&glLoadMatrixf},
    {"glMatrixMode", (uintptr_t)&glMatrixMode},
    {"glOrthof", (uintptr_t)&glOrthof},
    {"glPixelStorei", (uintptr_t)&glPixelStorei},
    {"glPopMatrix", (uintptr_t)&glPopMatrix},
    {"glPushMatrix", (uintptr_t)&glPushMatrix},
    {"glRotatef", (uintptr_t)&glRotatef},
    {"glScalef", (uintptr_t)&glScalef},
    {"glTexCoordPointer", (uintptr_t)&glTexCoordPointer},
    {"glTexEnvi", (uintptr_t)&glTexEnvi},
    {"glTexImage2D", (uintptr_t)&glTexImage2D},
    {"glTexParameteri", (uintptr_t)&glTexParameteri},
    {"glTexSubImage2D", (uintptr_t)&glTexSubImage2D},
    {"glTranslatef", (uintptr_t)&glTranslatef},
    {"glVertexPointer", (uintptr_t)&glVertexPointer},
    {"glViewport", (uintptr_t)&glViewport},
    {"glGetIntegerv", (uintptr_t)&glGetIntegerv},
    {"glGetFloatv", (uintptr_t)&glGetFloatv},

    // OpenGL ES 2.x functions
    {"glAttachShader", (uintptr_t)&glAttachShader},
    {"glBindBuffer", (uintptr_t)&glBindBuffer},
    {"glBufferData", (uintptr_t)&glBufferData},
    {"glBufferSubData", (uintptr_t)&glBufferSubData},
    {"glCompileShader", (uintptr_t)&glCompileShader},
    {"glCreateProgram", (uintptr_t)&glCreateProgram},
    {"glCreateShader", (uintptr_t)&glCreateShader},
    {"glDeleteBuffers", (uintptr_t)&glDeleteBuffers},
    {"glDeleteProgram", (uintptr_t)&glDeleteProgram},
    {"glDeleteShader", (uintptr_t)&glDeleteShader},
    {"glDetachShader", (uintptr_t)&glDetachShader_stub},
    {"glDisableVertexAttribArray", (uintptr_t)&glDisableVertexAttribArray},
    {"glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray},
    {"glGenBuffers", (uintptr_t)&glGenBuffers},
    {"glGetActiveAttrib", (uintptr_t)&glGetActiveAttrib},
    {"glGetActiveUniform", (uintptr_t)&glGetActiveUniform},
    {"glGetAttribLocation", (uintptr_t)&glGetAttribLocation},
    {"glGetProgramiv", (uintptr_t)&glGetProgramiv},
    {"glGetProgramInfoLog", (uintptr_t)&glGetProgramInfoLog},
    {"glGetShaderiv", (uintptr_t)&glGetShaderiv},
    {"glGetShaderInfoLog", (uintptr_t)&glGetShaderInfoLog},
    {"glGetUniformLocation", (uintptr_t)&glGetUniformLocation},
    {"glLinkProgram", (uintptr_t)&glLinkProgram},
    {"glShaderSource", (uintptr_t)&glShaderSource},
    {"glUniform1f", (uintptr_t)&glUniform1f},
    {"glUniform1i", (uintptr_t)&glUniform1i},
    {"glUniform2f", (uintptr_t)&glUniform2f},
    {"glUniform3f", (uintptr_t)&glUniform3f},
    {"glUniform4f", (uintptr_t)&glUniform4f},
    {"glUniformMatrix4fv", (uintptr_t)&glUniformMatrix4fv},
    {"glUseProgram", (uintptr_t)&glUseProgram},
    {"glVertexAttribPointer", (uintptr_t)&glVertexAttribPointer},

    // Android logging functions
    {"__android_log_print", (uintptr_t)&__android_log_print},
    {"__android_log_vprint", (uintptr_t)&__android_log_vprint},
    {"__android_log_assert", (uintptr_t)&__android_log_assert},

    // File I/O functions
    {"fopen", (uintptr_t)&fopen},
    {"fclose", (uintptr_t)&fclose},
    {"fread", (uintptr_t)&fread},
    {"fwrite", (uintptr_t)&fwrite},
    {"fseek", (uintptr_t)&fseek},
    {"ftell", (uintptr_t)&ftell},
    {"feof", (uintptr_t)&feof},
    {"ferror", (uintptr_t)&ferror},
    {"fflush", (uintptr_t)&fflush},
    {"fprintf", (uintptr_t)&fprintf},
    {"fscanf", (uintptr_t)&fscanf},
    {"fgetc", (uintptr_t)&fgetc},
    {"fgets", (uintptr_t)&fgets},
    {"fputc", (uintptr_t)&fputc},
    {"fputs", (uintptr_t)&fputs},
    {"mkdir", (uintptr_t)&mkdir},
    {"rmdir", (uintptr_t)&rmdir},

    // Printf family
    {"printf", (uintptr_t)&printf},
    {"sprintf", (uintptr_t)&sprintf},
    {"snprintf", (uintptr_t)&snprintf},
    {"vprintf", (uintptr_t)&vprintf},
    {"vsprintf", (uintptr_t)&vsprintf},
    {"vsnprintf", (uintptr_t)&vsnprintf},

    // Standard I/O
    {"stdin", (uintptr_t)&__sF_fake[0]},
    {"stdout", (uintptr_t)&__sF_fake[1]},
    {"stderr", (uintptr_t)&__sF_fake[2]},

    // Time functions
    {"time", (uintptr_t)&time},
    {"localtime", (uintptr_t)&localtime},
    {"gmtime", (uintptr_t)&gmtime},
    {"mktime", (uintptr_t)&mktime},
    {"strftime", (uintptr_t)&strftime},
    {"clock", (uintptr_t)&clock},
    {"gettimeofday", (uintptr_t)&gettimeofday},

    // Threading
    {"pthread_create", (uintptr_t)&pthread_create},
    {"pthread_join", (uintptr_t)&pthread_join},
    {"pthread_detach", (uintptr_t)&pthread_detach},
    {"pthread_mutex_init", (uintptr_t)&pthread_mutex_init},
    {"pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy},
    {"pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock},
    {"pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock},
    {"pthread_cond_init", (uintptr_t)&pthread_cond_init},
    {"pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy},
    {"pthread_cond_wait", (uintptr_t)&pthread_cond_wait},
    {"pthread_cond_signal", (uintptr_t)&pthread_cond_signal},
    {"pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast},

    // Error handling
    {"__errno_location", (uintptr_t)&__errno_location},
    {"strerror", (uintptr_t)&strerror},

    // Jump functions
    {"setjmp", (uintptr_t)&setjmp},
    {"longjmp", (uintptr_t)&longjmp},

    // Signals
    {"signal", (uintptr_t)&signal},
    {"raise", (uintptr_t)&raise},

    // Locale
    {"setlocale", (uintptr_t)&setlocale},

    // Network (basic stubs)
    {"gethostname", (uintptr_t)&gethostname},

    // zlib functions
    {"adler32", (uintptr_t)&adler32},
    {"compress", (uintptr_t)&compress},
    {"compressBound", (uintptr_t)&compressBound},
    {"crc32", (uintptr_t)&crc32},
    {"deflate", (uintptr_t)&deflate},
    {"deflateEnd", (uintptr_t)&deflateEnd},
    {"deflateInit2_", (uintptr_t)&deflateInit2_},
    {"deflateInit_", (uintptr_t)&deflateInit_},
    {"deflateReset", (uintptr_t)&deflateReset},
    {"inflate", (uintptr_t)&inflate},
    {"inflateEnd", (uintptr_t)&inflateEnd},
    {"inflateInit2_", (uintptr_t)&inflateInit2_},
    {"inflateInit_", (uintptr_t)&inflateInit_},
    {"inflateReset", (uintptr_t)&inflateReset},
    {"uncompress", (uintptr_t)&uncompress},

    // Dynamic loading
    {"dlopen", (uintptr_t)&dlopen},
    {"dlclose", (uintptr_t)&dlclose},
    {"dlsym", (uintptr_t)&dlsym},
    {"dlerror", (uintptr_t)&dlerror},
};

void resolve_imports(so_module* mod) {
    l_info("Resolving imports for Fluffy Diver");

    // Set up fake stdio
    __sF_fake[0] = *stdin;
    __sF_fake[1] = *stdout;
    __sF_fake[2] = *stderr;

    // Resolve symbols using our custom table
    so_resolve(mod, default_dynlib, sizeof(default_dynlib) / sizeof(so_default_dynlib), 0);

    l_success("Symbol resolution complete - %d symbols resolved",
              (int)(sizeof(default_dynlib) / sizeof(so_default_dynlib)));
}
