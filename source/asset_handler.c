/*
 * Fluffy Diver PS Vita Port
 * Custom Asset Format Handler
 *
 * Handles the proprietary game engine formats:
 * - .hgg files (compressed game data)
 * - .spr files (sprite/texture data)
 * - .hif files (image data)
 * - .hdm files (map/level data)
 * - .yfont files (font data)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/clib.h>

#include "utils/logger.h"
#include "utils/utils.h"

// Asset format signatures
#define HGG_SIGNATURE   0x48474700  // "HGG\0"
#define SPR_SIGNATURE   0x53505200  // "SPR\0"
#define HIF_SIGNATURE   0x48494600  // "HIF\0"
#define HDM_SIGNATURE   0x48444D00  // "HDM\0"

// Asset cache management
typedef struct {
    char filename[256];
    void *data;
    size_t size;
    int format;
    int loaded;
} asset_cache_entry_t;

#define MAX_CACHED_ASSETS 512
static asset_cache_entry_t asset_cache[MAX_CACHED_ASSETS];
static int cache_count = 0;

// Asset format types
typedef enum {
    ASSET_FORMAT_UNKNOWN = 0,
    ASSET_FORMAT_HGG,
    ASSET_FORMAT_SPR,
    ASSET_FORMAT_HIF,
    ASSET_FORMAT_HDM,
    ASSET_FORMAT_YFONT,
    ASSET_FORMAT_PNG,
    ASSET_FORMAT_DAT
} asset_format_t;

// Function prototypes
static asset_format_t detect_asset_format(const char *filename);
static int load_hgg_file(const char *path, void **data, size_t *size);
static int load_spr_file(const char *path, void **data, size_t *size);
static int load_hif_file(const char *path, void **data, size_t *size);
static int load_hdm_file(const char *path, void **data, size_t *size);
static int load_yfont_file(const char *path, void **data, size_t *size);
static int load_png_file(const char *path, void **data, size_t *size);
static int load_dat_file(const char *path, void **data, size_t *size);
static void *get_cached_asset(const char *filename);
static void cache_asset(const char *filename, void *data, size_t size, int format);

// Initialize asset system
int init_asset_system(void) {
    l_info("Initializing Fluffy Diver asset system");

    // Clear asset cache
    sceClibMemset(asset_cache, 0, sizeof(asset_cache));
    cache_count = 0;

    // Verify asset directory exists
    if (!file_exists("ux0:data/fluffydiver/assets/")) {
        l_error("Asset directory not found: ux0:data/fluffydiver/assets/");
        return -1;
    }

    // Pre-load critical assets
    preload_critical_assets();

    l_success("Asset system initialized");
    return 0;
}

// Main asset loading function
void* load_asset(const char *filename, size_t *out_size) {
    if (!filename || !out_size) {
        l_error("Invalid parameters to load_asset");
        return NULL;
    }

    // Check cache first
    void *cached_data = get_cached_asset(filename);
    if (cached_data) {
        l_debug("Asset loaded from cache: %s", filename);
        return cached_data;
    }

    // Build full path
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "ux0:data/fluffydiver/assets/%s", filename);

    // Detect format
    asset_format_t format = detect_asset_format(filename);

    void *data = NULL;
    size_t size = 0;
    int result = -1;

    // Load based on format
    switch (format) {
        case ASSET_FORMAT_HGG:
            result = load_hgg_file(full_path, &data, &size);
            break;
        case ASSET_FORMAT_SPR:
            result = load_spr_file(full_path, &data, &size);
            break;
        case ASSET_FORMAT_HIF:
            result = load_hif_file(full_path, &data, &size);
            break;
        case ASSET_FORMAT_HDM:
            result = load_hdm_file(full_path, &data, &size);
            break;
        case ASSET_FORMAT_YFONT:
            result = load_yfont_file(full_path, &data, &size);
            break;
        case ASSET_FORMAT_PNG:
            result = load_png_file(full_path, &data, &size);
            break;
        case ASSET_FORMAT_DAT:
            result = load_dat_file(full_path, &data, &size);
            break;
        default:
            l_warning("Unknown asset format: %s", filename);
            result = -1;
            break;
    }

    if (result == 0 && data) {
        *out_size = size;
        cache_asset(filename, data, size, format);
        l_debug("Asset loaded: %s (%zu bytes)", filename, size);
        return data;
    }

    l_error("Failed to load asset: %s", filename);
    return NULL;
}

// Detect asset format from filename
static asset_format_t detect_asset_format(const char *filename) {
    if (!filename) return ASSET_FORMAT_UNKNOWN;

    const char *ext = strrchr(filename, '.');
    if (!ext) return ASSET_FORMAT_UNKNOWN;

    if (strcmp(ext, ".hgg") == 0) return ASSET_FORMAT_HGG;
    if (strcmp(ext, ".spr") == 0) return ASSET_FORMAT_SPR;
    if (strcmp(ext, ".hif") == 0) return ASSET_FORMAT_HIF;
    if (strcmp(ext, ".hdm") == 0) return ASSET_FORMAT_HDM;
    if (strcmp(ext, ".yfont") == 0) return ASSET_FORMAT_YFONT;
    if (strcmp(ext, ".png") == 0) return ASSET_FORMAT_PNG;
    if (strcmp(ext, ".dat") == 0) return ASSET_FORMAT_DAT;

    return ASSET_FORMAT_UNKNOWN;
}

// Load HGG files (compressed game data)
static int load_hgg_file(const char *path, void **data, size_t *size) {
    l_debug("Loading HGG file: %s", path);

    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        l_error("Failed to open HGG file: %s", path);
        return -1;
    }

    // Get file size
    SceIoStat stat;
    sceIoGetstatByFd(fd, &stat);
    *size = stat.st_size;

    // Allocate memory
    *data = malloc(*size);
    if (!*data) {
        sceIoClose(fd);
        return -1;
    }

    // Read file
    int bytes_read = sceIoRead(fd, *data, *size);
    sceIoClose(fd);

    if (bytes_read != *size) {
        free(*data);
        *data = NULL;
        return -1;
    }

    // HGG files might be compressed - check signature and decompress if needed
    uint32_t *header = (uint32_t*)*data;
    if (*header == HGG_SIGNATURE) {
        // This is a compressed HGG file - implement decompression
        l_debug("HGG file is compressed, decompressing...");
        // TODO: Implement HGG decompression based on format analysis
    }

    return 0;
}

// Load SPR files (sprite data)
static int load_spr_file(const char *path, void **data, size_t *size) {
    l_debug("Loading SPR file: %s", path);

    // SPR files are likely sprite/texture data
    // Load as binary data for now
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        l_error("Failed to open SPR file: %s", path);
        return -1;
    }

    SceIoStat stat;
    sceIoGetstatByFd(fd, &stat);
    *size = stat.st_size;

    *data = malloc(*size);
    if (!*data) {
        sceIoClose(fd);
        return -1;
    }

    int bytes_read = sceIoRead(fd, *data, *size);
    sceIoClose(fd);

    return (bytes_read == *size) ? 0 : -1;
}

// Load HIF files (image data)
static int load_hif_file(const char *path, void **data, size_t *size) {
    l_debug("Loading HIF file: %s", path);

    // HIF files are likely image format
    // Load as binary data for now
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        l_error("Failed to open HIF file: %s", path);
        return -1;
    }

    SceIoStat stat;
    sceIoGetstatByFd(fd, &stat);
    *size = stat.st_size;

    *data = malloc(*size);
    if (!*data) {
        sceIoClose(fd);
        return -1;
    }

    int bytes_read = sceIoRead(fd, *data, *size);
    sceIoClose(fd);

    return (bytes_read == *size) ? 0 : -1;
}

// Load HDM files (map/level data)
static int load_hdm_file(const char *path, void **data, size_t *size) {
    l_debug("Loading HDM file: %s", path);

    // HDM files are likely map/level data
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        l_error("Failed to open HDM file: %s", path);
        return -1;
    }

    SceIoStat stat;
    sceIoGetstatByFd(fd, &stat);
    *size = stat.st_size;

    *data = malloc(*size);
    if (!*data) {
        sceIoClose(fd);
        return -1;
    }

    int bytes_read = sceIoRead(fd, *data, *size);
    sceIoClose(fd);

    return (bytes_read == *size) ? 0 : -1;
}

// Load YFONT files (font data)
static int load_yfont_file(const char *path, void **data, size_t *size) {
    l_debug("Loading YFONT file: %s", path);

    // YFONT files are font data
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        l_error("Failed to open YFONT file: %s", path);
        return -1;
    }

    SceIoStat stat;
    sceIoGetstatByFd(fd, &stat);
    *size = stat.st_size;

    *data = malloc(*size);
    if (!*data) {
        sceIoClose(fd);
        return -1;
    }

    int bytes_read = sceIoRead(fd, *data, *size);
    sceIoClose(fd);

    return (bytes_read == *size) ? 0 : -1;
}

// Load PNG files (standard images)
static int load_png_file(const char *path, void **data, size_t *size) {
    l_debug("Loading PNG file: %s", path);

    // Standard PNG loading
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        l_error("Failed to open PNG file: %s", path);
        return -1;
    }

    SceIoStat stat;
    sceIoGetstatByFd(fd, &stat);
    *size = stat.st_size;

    *data = malloc(*size);
    if (!*data) {
        sceIoClose(fd);
        return -1;
    }

    int bytes_read = sceIoRead(fd, *data, *size);
    sceIoClose(fd);

    return (bytes_read == *size) ? 0 : -1;
}

// Load DAT files (data files)
static int load_dat_file(const char *path, void **data, size_t *size) {
    l_debug("Loading DAT file: %s", path);

    // DAT files are general data
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        l_error("Failed to open DAT file: %s", path);
        return -1;
    }

    SceIoStat stat;
    sceIoGetstatByFd(fd, &stat);
    *size = stat.st_size;

    *data = malloc(*size);
    if (!*data) {
        sceIoClose(fd);
        return -1;
    }

    int bytes_read = sceIoRead(fd, *data, *size);
    sceIoClose(fd);

    return (bytes_read == *size) ? 0 : -1;
}

// Get cached asset
static void *get_cached_asset(const char *filename) {
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(asset_cache[i].filename, filename) == 0 && asset_cache[i].loaded) {
            return asset_cache[i].data;
        }
    }
    return NULL;
}

// Cache asset
static void cache_asset(const char *filename, void *data, size_t size, int format) {
    if (cache_count >= MAX_CACHED_ASSETS) {
        l_warning("Asset cache full, not caching: %s", filename);
        return;
    }

    strncpy(asset_cache[cache_count].filename, filename, sizeof(asset_cache[cache_count].filename) - 1);
    asset_cache[cache_count].data = data;
    asset_cache[cache_count].size = size;
    asset_cache[cache_count].format = format;
    asset_cache[cache_count].loaded = 1;
    cache_count++;
}

// Pre-load critical assets
static void preload_critical_assets(void) {
    l_info("Pre-loading critical assets");

    // Load essential files that are likely needed at startup
    const char *critical_assets[] = {
        "data.dat",
        "fluffy.png",
        "default.png",
        "ui_common.spr",
        "font_16.yfont",
        "font_28.yfont",
        NULL
    };

    for (int i = 0; critical_assets[i]; i++) {
        size_t size;
        void *data = load_asset(critical_assets[i], &size);
        if (data) {
            l_debug("Pre-loaded critical asset: %s", critical_assets[i]);
        }
    }
}

// Cleanup asset system
void cleanup_asset_system(void) {
    l_info("Cleaning up asset system");

    for (int i = 0; i < cache_count; i++) {
        if (asset_cache[i].data) {
            free(asset_cache[i].data);
            asset_cache[i].data = NULL;
        }
    }

    cache_count = 0;
    l_success("Asset system cleaned up");
}
