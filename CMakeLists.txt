cmake_minimum_required(VERSION 3.19)

# Set toolchain for PS Vita
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif()
endif()

# Project configuration
project(FluffyDiver)
include("${VITASDK}/share/vita.cmake" REQUIRED)

# Game metadata
set(VITA_APP_NAME "Fluffy Diver")
set(VITA_TITLEID  "FLUF00001")
set(VITA_VERSION  "01.00")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")  # Extended memory mode

# Compiler flags for ARM with soft-float ABI (required for VitaGL)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp -O3 -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfloat-abi=softfp -O3 -Wall -Wextra")

# Add DEBUG flag for development
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG")

# Define data path
add_definitions(-DDATA_PATH="ux0:data/fluffydiver/")

# Source files - Phase 2 with Graphics and Audio
add_executable(${CMAKE_PROJECT_NAME}
               # Phase 2 main files
               source/main.c
               source/dynlib.c
               source/patch.c
               source/fluffydiver_jni.c
               source/asset_handler.c
               source/java.c

               # Phase 2 NEW: Graphics and Audio systems
               source/graphics.c
               source/audio.c

               # Boilerplate files (unchanged)
               source/reimpl/errno.c
               source/reimpl/io.c
               source/reimpl/log.c
               source/reimpl/mem.c
               source/reimpl/pthr.c
               source/reimpl/sys.c
               source/reimpl/time64.c
               source/reimpl/asset_manager.cpp
               source/utils/dialog.c
               source/utils/glutil.c
               source/utils/init.c
               source/utils/logger.c
               source/utils/settings.c
               source/utils/utils.c

               # Required libraries
               lib/falso_jni/FalsoJNI.c
               lib/falso_jni/FalsoJNI_ImplBridge.c
               lib/falso_jni/FalsoJNI_Logger.c
               lib/sha1/sha1.c
               lib/fios/fios.c
               lib/so_util/so_util.c)

# Build libc bridge
add_subdirectory(lib/libc_bridge)
add_dependencies(${CMAKE_PROJECT_NAME} SceLibcBridge)

# Include directories
target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM
                           PUBLIC ${CMAKE_SOURCE_DIR}/lib
                           PUBLIC ${CMAKE_SOURCE_DIR}/source)

target_include_directories(${CMAKE_PROJECT_NAME}
                           PUBLIC ${CMAKE_SOURCE_DIR}/source
                           PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Link libraries - Phase 2 with Graphics and Audio support
target_link_libraries(${CMAKE_PROJECT_NAME}
                      # Essential libraries
                      -Wl,--whole-archive pthread -Wl,--no-whole-archive
                      stdc++

                      # Phase 2 NEW: Graphics libraries (VitaGL, shaders)
                      vitaGL
                      vitashark
                      SceShaccCgExt
                      SceShaccCg_stub

                      # Phase 2 NEW: Audio libraries (OpenAL, audio formats)
                      openal
                      vorbisfile
                      vorbis
                      ogg

                      # Math and utility libraries
                      mathneon
                      z
                      m

                      # Vita-specific libraries
                      taihen_stub
                      kubridge_stub
                      ${CMAKE_BINARY_DIR}/lib/libc_bridge/stubs/libSceLibcBridge_stub.a

                      # Vita system stubs - Extended for Phase 2
                      SceHid_stub
                      SceTouch_stub
                      SceMotion_stub
                      SceAppMgr_stub
                      SceAppUtil_stub
                      SceAudio_stub
                      SceAudioIn_stub
                      SceAudioOut_stub
                      SceCommonDialog_stub
                      SceDisplay_stub
                      SceKernelDmacMgr_stub
                      SceFios2_stub
                      SceCtrl_stub
                      SceGxm_stub
                      ScePower_stub
                      SceTouch_stub
                      SceVshBridge_stub
                      ScePgf_stub
                      SceKernelModulemgr_stub
                      SceIofilemgr_stub
                      SceNet_stub
                      SceNetCtl_stub
                      SceHttp_stub
                      SceSsl_stub
                      SceSystemGesture_stub
                      SceKernelThreadMgr_stub
                      SceKernelSysmem_stub
                      SceKernelProcessMgr_stub)

# Create self and VPK with Phase 2 assets
vita_create_self(eboot.bin ${CMAKE_PROJECT_NAME} UNSAFE NOASLR)
vita_create_vpk(${CMAKE_PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
                VERSION ${VITA_VERSION}
                NAME ${VITA_APP_NAME}

                # Boilerplate files
                FILE ${CMAKE_SOURCE_DIR}/extras/cpuinfo cpuinfo
                ${CMAKE_SOURCE_DIR}/extras/gamecontrollerdb.txt gamecontrollerdb.txt

                # LiveArea assets
                FILE ${CMAKE_SOURCE_DIR}/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
                ${CMAKE_SOURCE_DIR}/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
                ${CMAKE_SOURCE_DIR}/livearea/contents/icon0.png sce_sys/icon0.png
                ${CMAKE_SOURCE_DIR}/livearea/contents/template.xml sce_sys/livearea/contents/template.xml)

# Build configuration summary
message(STATUS "=== Fluffy Diver Phase 2 Build Configuration ===")
message(STATUS "Title ID: ${VITA_TITLEID}")
message(STATUS "Version: ${VITA_VERSION}")
message(STATUS "Extended Memory: Enabled")
message(STATUS "Graphics: VitaGL + VitaShark")
message(STATUS "Audio: OpenAL-soft + Vorbis")
message(STATUS "Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "C Flags: ${CMAKE_C_FLAGS}")
message(STATUS "CXX Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "============================================")

# Post-build instructions
add_custom_target(install_instructions ALL
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=== Phase 2 Installation Instructions ==="
    COMMAND ${CMAKE_COMMAND} -E echo "1. Copy FluffyDiver.vpk to your PS Vita"
    COMMAND ${CMAKE_COMMAND} -E echo "2. Install with VitaShell"
    COMMAND ${CMAKE_COMMAND} -E echo "3. Copy libFluffyDiver.so to ux0:data/fluffydiver/"
    COMMAND ${CMAKE_COMMAND} -E echo "4. Copy all assets to ux0:data/fluffydiver/assets/"
    COMMAND ${CMAKE_COMMAND} -E echo "5. Required plugins:"
    COMMAND ${CMAKE_COMMAND} -E echo "   - kubridge v0.3.1+"
    COMMAND ${CMAKE_COMMAND} -E echo "   - fd_fix"
    COMMAND ${CMAKE_COMMAND} -E echo "   - libshacccg.suprx"
    COMMAND ${CMAKE_COMMAND} -E echo "6. Reboot and launch!"
    COMMAND ${CMAKE_COMMAND} -E echo "========================================="
    DEPENDS ${CMAKE_PROJECT_NAME}
)

# Development helpers
add_custom_target(debug_build
    COMMAND ${CMAKE_COMMAND} -E echo "Building debug version..."
    COMMAND ${CMAKE_COMMAND} --build . --target ${CMAKE_PROJECT_NAME}
    DEPENDS ${CMAKE_PROJECT_NAME}
)

add_custom_target(clean_build
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning build directory..."
    COMMAND ${CMAKE_COMMAND} --build . --target clean
    COMMAND ${CMAKE_COMMAND} -E echo "Clean complete"
)

# Validation checks
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/source/graphics.c")
    message(WARNING "Graphics system not found - Phase 2 graphics will not work")
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/source/audio.c")
    message(WARNING "Audio system not found - Phase 2 audio will not work")
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/include/config.h")
    message(WARNING "Config header not found - using defaults")
endif()

# Asset validation
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/livearea/contents/icon0.png")
    message(WARNING "LiveArea icon missing - using default")
endif()

# Memory configuration warning
message(STATUS "")
message(STATUS "IMPORTANT: This build uses extended memory mode")
message(STATUS "Ensure your PS Vita has adequate free memory")
message(STATUS "Recommended: Close other applications before launching")
message(STATUS "")
