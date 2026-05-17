#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# LYN4XP / Flux Pro Engine — Build Script
# Compiles library.so (arm64-v8a) for 8 Ball Pool mod
# ─────────────────────────────────────────────────────────────────────────────

set -e

NDK_VERSION="r25c"
NDK_DIR="/tmp/android-ndk-${NDK_VERSION}"
NDK_ZIP="/tmp/ndk.zip"
NDK_URL="https://dl.google.com/android/repository/android-ndk-${NDK_VERSION}-linux.zip"
JNI_DIR="app/src/main/jni"
OUT_DIR="${JNI_DIR}/libs/arm64-v8a"
OUT_SO="${OUT_DIR}/library.so"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}╔══════════════════════════════════════════╗${NC}"
echo -e "${YELLOW}║   LYN4XP Build Script — arm64-v8a       ║${NC}"
echo -e "${YELLOW}╚══════════════════════════════════════════╝${NC}"
echo ""

# ── 1. Check / Download NDK ──────────────────────────────────────────────────
if [ ! -d "$NDK_DIR" ]; then
    if [ ! -f "$NDK_ZIP" ]; then
        echo -e "${YELLOW}[1/3] Downloading Android NDK ${NDK_VERSION}...${NC}"
        wget -q --show-progress -O "$NDK_ZIP" "$NDK_URL"
    fi
    echo -e "${YELLOW}[1/3] Extracting NDK...${NC}"
    unzip -q "$NDK_ZIP" -d /tmp/
    echo -e "${GREEN}      NDK ready at ${NDK_DIR}${NC}"
else
    echo -e "${GREEN}[1/3] NDK already present — skipping download${NC}"
fi

# ── 2. Build ─────────────────────────────────────────────────────────────────
echo ""
echo -e "${YELLOW}[2/3] Running ndk-build...${NC}"
echo ""

"${NDK_DIR}/ndk-build" \
    NDK_PROJECT_PATH="${JNI_DIR}" \
    APP_BUILD_SCRIPT="${JNI_DIR}/Android.mk" \
    NDK_APPLICATION_MK="${JNI_DIR}/Application.mk" \
    -C "${JNI_DIR}" \
    2>&1

# ── 3. Report ────────────────────────────────────────────────────────────────
echo ""
if [ -f "$OUT_SO" ]; then
    SIZE=$(du -sh "$OUT_SO" | cut -f1)
    MD5=$(md5sum "$OUT_SO" | cut -d' ' -f1)
    echo -e "${GREEN}[3/3] BUILD SUCCESS ✓${NC}"
    echo -e "      File : ${OUT_SO}"
    echo -e "      Size : ${SIZE}"
    echo -e "      MD5  : ${MD5}"
    echo ""
    echo -e "${YELLOW}      → Copy library.so to your 8BP mod folder:${NC}"
    echo -e "         /data/app/com.miniclip.eightballpool-*/lib/arm64/"
else
    echo -e "${RED}[3/3] BUILD FAILED — library.so not found${NC}"
    exit 1
fi
