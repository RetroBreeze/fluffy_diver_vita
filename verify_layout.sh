#!/bin/bash

# Fluffy Diver PS Vita Port - Layout Verification Script
# Run this in your fluffy_diver_vita directory to verify the structure

echo "=========================================="
echo "Fluffy Diver PS Vita Port - Layout Check"
echo "=========================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
missing_files=0
incorrect_files=0
correct_files=0

# Function to check if file exists
check_file() {
    local file="$1"
    local status="$2"
    local description="$3"
    
    if [ -f "$file" ]; then
        if [ "$status" = "REQUIRED" ]; then
            echo -e "${GREEN}✓${NC} $file ${BLUE}($description)${NC}"
            ((correct_files++))
        elif [ "$status" = "KEEP_BOILERPLATE" ]; then
            echo -e "${YELLOW}✓${NC} $file ${BLUE}($description)${NC}"
            ((correct_files++))
        else
            echo -e "${GREEN}✓${NC} $file ${BLUE}($description)${NC}"
            ((correct_files++))
        fi
    else
        echo -e "${RED}✗${NC} $file ${BLUE}($description)${NC}"
        ((missing_files++))
    fi
}

# Function to check if directory exists
check_dir() {
    local dir="$1"
    local description="$2"
    
    if [ -d "$dir" ]; then
        echo -e "${GREEN}✓${NC} $dir/ ${BLUE}($description)${NC}"
        ((correct_files++))
    else
        echo -e "${RED}✗${NC} $dir/ ${BLUE}($description)${NC}"
        ((missing_files++))
    fi
}

# Function to check file content for specific patterns
check_content() {
    local file="$1"
    local pattern="$2"
    local description="$3"
    
    if [ -f "$file" ]; then
        if grep -q "$pattern" "$file" 2>/dev/null; then
            echo -e "${GREEN}✓${NC} $file contains '$pattern' ${BLUE}($description)${NC}"
            ((correct_files++))
        else
            echo -e "${RED}✗${NC} $file missing '$pattern' ${BLUE}($description)${NC}"
            ((incorrect_files++))
        fi
    else
        echo -e "${RED}✗${NC} $file not found ${BLUE}($description)${NC}"
        ((missing_files++))
    fi
}

echo -e "\n${BLUE}=== PHASE 1 ARTIFACTS (My Implementation) ===${NC}"
check_file "CMakeLists.txt" "REQUIRED" "Main build configuration - should contain FLUF00001"
check_file "source/main.c" "REQUIRED" "Main entry point - should contain fluffy_diver_state_t"
check_file "source/dynlib.c" "REQUIRED" "Symbol resolution - should contain 45 OpenGL functions"
check_file "source/patch.c" "REQUIRED" "Game patches - should contain fluffy_state"
check_file "source/fluffydiver_jni.c" "REQUIRED" "JNI implementation - should contain 13 JNI functions"
check_file "include/config.h" "REQUIRED" "Configuration header - should contain FLUF00001"

echo -e "\n${BLUE}=== BOILERPLATE FILES (Keep Unchanged) ===${NC}"
check_file "source/java.c" "KEEP_BOILERPLATE" "FalsoJNI method registry"
check_file "source/reimpl/io.c" "KEEP_BOILERPLATE" "I/O reimplementation"
check_file "source/reimpl/mem.c" "KEEP_BOILERPLATE" "Memory reimplementation"
check_file "source/reimpl/log.c" "KEEP_BOILERPLATE" "Logging reimplementation"
check_file "source/utils/init.c" "KEEP_BOILERPLATE" "Initialization utilities"
check_file "source/utils/logger.c" "KEEP_BOILERPLATE" "Logger utilities"
check_file "source/utils/glutil.c" "KEEP_BOILERPLATE" "OpenGL utilities"

echo -e "\n${BLUE}=== DIRECTORY STRUCTURE ===${NC}"
check_dir "source/reimpl" "Reimplementation files"
check_dir "source/utils" "Utility files"
check_dir "lib/falso_jni" "FalsoJNI library (from submodule)"
check_dir "lib/so_util" "So-loader utilities"
check_dir "lib/libc_bridge" "Libc bridge"
check_dir "livearea" "PS Vita LiveArea assets"
check_dir "extras" "Extra files"

echo -e "\n${BLUE}=== CRITICAL SUBMODULE FILES ===${NC}"
check_file "lib/falso_jni/FalsoJNI.c" "REQUIRED" "FalsoJNI main implementation"
check_file "lib/falso_jni/FalsoJNI_ImplBridge.c" "REQUIRED" "FalsoJNI bridge implementation"
check_file "lib/falso_jni/FalsoJNI_Logger.c" "REQUIRED" "FalsoJNI logging"
check_file "lib/so_util/so_util.c" "REQUIRED" "So-loader utilities"
check_file "lib/libc_bridge/CMakeLists.txt" "REQUIRED" "Libc bridge build"

echo -e "\n${BLUE}=== CONFIGURATION FILES ===${NC}"
check_file ".gitmodules" "REQUIRED" "Git submodule configuration"
check_file ".gitignore" "REQUIRED" "Git ignore rules"

echo -e "\n${BLUE}=== CONTENT VERIFICATION ===${NC}"
check_content "CMakeLists.txt" "FLUF00001" "Should contain Fluffy Diver title ID"
check_content "CMakeLists.txt" "FluffyDiver" "Should contain project name"
check_content "source/main.c" "fluffy_diver_state_t" "Should contain game state structure"
check_content "source/fluffydiver_jni.c" "Java_com_hotdog_jni_Natives_OnGameInitialize" "Should contain JNI functions"
check_content "source/dynlib.c" "glActiveTexture" "Should contain OpenGL function mapping"
check_content "include/config.h" "FLUF00001" "Should contain title ID"

echo -e "\n${BLUE}=== SUBMODULE STATUS ===${NC}"
if [ -d ".git" ]; then
    echo "Git submodule status:"
    git submodule status
else
    echo -e "${YELLOW}⚠${NC} Not a git repository - submodules may not be initialized"
fi

echo -e "\n${BLUE}=== GAME DATA DIRECTORIES ===${NC}"
check_dir "data" "Game data directory (create if missing)"
if [ -f "lib/libFluffyDiver.so" ]; then
    echo -e "${GREEN}✓${NC} lib/libFluffyDiver.so ${BLUE}(Game library present)${NC}"
    ((correct_files++))
else
    echo -e "${YELLOW}⚠${NC} lib/libFluffyDiver.so ${BLUE}(Game library - copy from APK)${NC}"
fi

if [ -d "data/assets" ]; then
    asset_count=$(find data/assets -type f 2>/dev/null | wc -l)
    echo -e "${GREEN}✓${NC} data/assets/ ${BLUE}($asset_count files - extract from APK)${NC}"
    ((correct_files++))
else
    echo -e "${YELLOW}⚠${NC} data/assets/ ${BLUE}(Asset directory - extract from APK)${NC}"
fi

echo -e "\n=========================================="
echo -e "${BLUE}SUMMARY:${NC}"
echo -e "${GREEN}✓ Correct files: $correct_files${NC}"
echo -e "${RED}✗ Missing files: $missing_files${NC}"
echo -e "${RED}✗ Incorrect files: $incorrect_files${NC}"

if [ $missing_files -eq 0 ] && [ $incorrect_files -eq 0 ]; then
    echo -e "\n${GREEN}🎉 PROJECT STRUCTURE IS CORRECT!${NC}"
    echo -e "${BLUE}Ready for Phase 2 implementation.${NC}"
    exit 0
elif [ $missing_files -gt 0 ]; then
    echo -e "\n${RED}❌ MISSING FILES DETECTED${NC}"
    echo -e "${YELLOW}Run the following commands to fix:${NC}"
    echo -e "${BLUE}git submodule update --init --recursive${NC}"
    echo -e "${BLUE}mkdir -p include data/assets${NC}"
    exit 1
else
    echo -e "\n${YELLOW}⚠ INCORRECT FILE CONTENT${NC}"
    echo -e "${BLUE}Check the files marked with ✗ above${NC}"
    exit 1
fi