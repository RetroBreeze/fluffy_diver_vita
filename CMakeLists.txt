cmake_minimum_required(VERSION 3.19)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif()
endif()

project(FluffyDiver)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Fluffy Diver")
set(VITA_TITLEID  "FLUF00001")
set(VITA_VERSION  "01.00")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")  # Extended memory mode for larger games

# Game-specific configuration
set(SO_PATH "ux0:data/fluffydiver/libFluffyDiver.so")
set(DATA_PATH "ux0:data/fluffydiver")
set(ASSETS_PATH "ux0:data/fluffydiver/assets")

# Compiler flags optimized for game performance
add_definitions(-D_GNU_SOURCE -D__POSIX_VISIBLE=999999)
add_definitions(-DSO_PATH=\"${SO_PATH}\")
add_definitions(-DDATA_PATH=\"${DATA_PATH}\")
add_definitions(-DASSETS_PATH=\"${ASSETS_PATH}\")

# Fluffy Diver specific defines
add_definitions(-DGAME_TITLE_ID=\"${VITA_TITLEID}\")
add_definitions(-DGAME_NAME=\"${VITA_APP_NAME}\")
add_definitions(-DLOAD_ADDRESS=0x98000000)

# Performance and compatibility flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3 -g -ffast-math -mfloat-abi=softfp -Wno-deprecated")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=gnu++20 -Wno-write-strings -Wno-psabi")

# Memory management wrappers for stability
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--wrap,malloc,--wrap,free,--wrap,calloc,--wrap,realloc")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--wrap,memcpy,--wrap,memmove,--wrap,memset")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--wrap,memchr,--wrap,memcmp,--wrap,strlen,--wrap,strcpy")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--wrap,strncpy,--wrap,strcat,--wrap,strncat,--wrap,strcmp")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--wrap,strncmp,--wrap,strchr,--wrap,strrchr,--wrap,strstr")

# Source files for Fluffy Diver port
add_executable(${CMAKE_PROJECT_NAME}
               source/main.c
               source/dynlib.c
               source/java.c
               source/patch.c
               source/fluffydiver_jni.c
               source/reimpl/bits/_ctype.c
               source/reimpl/egl.c
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
               source/asset_handler.c
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

# Link libraries required for Fluffy Diver
target_link_libraries(${CMAKE_PROJECT_NAME}
                      # Essential libraries
                      -Wl,--whole-archive pthread -Wl,--no-whole-archive
                      stdc++

                      # Graphics libraries
                      vitaGL
                      vitashark
                      SceShaccCgExt
                      SceShaccCg_stub

                      # Audio libraries
                      openal
                      vorbisfile
                      vorbis
                      ogg

                      # System libraries
                      mathneon
                      z
                      m

                      # Vita-specific libraries
                      taihen_stub
                      kubridge_stub
                      ${CMAKE_BINARY_DIR}/lib/libc_bridge/stubs/libSceLibcBridge_stub.a

                      # Vita system stubs
                      SceHid_stub
                      SceTouch_stub
                      SceMotion_stub
                      SceAppMgr_stub
                      SceAppUtil_stub
                      SceAudio_stub
                      SceAudioIn_stub
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
                      SceSsl_stub)

# Create self and VPK
vita_create_self(eboot.bin ${CMAKE_PROJECT_NAME} UNSAFE NOASLR)
vita_create_vpk(${CMAKE_PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
                VERSION ${VITA_VERSION}
                NAME ${VITA_APP_NAME}
                FILE ${CMAKE_SOURCE_DIR}/extras/cpuinfo cpuinfo
                ${CMAKE_SOURCE_DIR}/extras/gamecontrollerdb.txt gamecontrollerdb.txt
                FILE ${CMAKE_SOURCE_DIR}/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
                ${CMAKE_SOURCE_DIR}/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
                ${CMAKE_SOURCE_DIR}/livearea/contents/icon0.png sce_sys/icon0.png
                ${CMAKE_SOURCE_DIR}/livearea/contents/template.xml sce_sys/livearea/contents/template.xml)
