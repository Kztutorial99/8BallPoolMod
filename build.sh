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
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
JNI_DIR="${ROOT_DIR}/app/src/main/jni"
OUT_DIR="${JNI_DIR}/libs/arm64-v8a"
OUT_SO="${OUT_DIR}/library.so"
OBJ_DIR="${JNI_DIR}/obj"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}╔══════════════════════════════════════════╗${NC}"
echo -e "${YELLOW}║   LYN4XP Build Script — arm64-v8a       ║${NC}"
echo -e "${YELLOW}╚══════════════════════════════════════════╝${NC}"
echo ""

# ── 0. Clean old build cache ──────────────────────────────────────────────────
echo -e "${YELLOW}[0/4] Cleaning old build cache...${NC}"
rm -rf "${OUT_DIR}" "${OBJ_DIR}"
mkdir -p "${OUT_DIR}"
echo -e "${GREEN}      Cache cleaned${NC}"
echo ""

# ── 1. Check / Download NDK ──────────────────────────────────────────────────
if [ ! -d "$NDK_DIR" ]; then
    if [ ! -f "$NDK_ZIP" ]; then
        echo -e "${YELLOW}[1/4] Downloading Android NDK ${NDK_VERSION}...${NC}"
        wget -q --show-progress -O "$NDK_ZIP" "$NDK_URL"
    fi
    echo -e "${YELLOW}[1/4] Extracting NDK...${NC}"
    unzip -q "$NDK_ZIP" -d /tmp/
    echo -e "${GREEN}      NDK ready at ${NDK_DIR}${NC}"
else
    echo -e "${GREEN}[1/4] NDK already present — skipping download${NC}"
fi

# ── 2. Build ─────────────────────────────────────────────────────────────────
echo ""
echo -e "${YELLOW}[2/4] Running ndk-build (fresh build)...${NC}"
echo ""

# NDK r25c needs toolchain/llvm-project/libcxxabi symlink for c++_static to work
if [ ! -d "${NDK_DIR}/toolchain/llvm-project/libcxxabi" ]; then
    mkdir -p "${NDK_DIR}/toolchain/llvm-project"
    ln -sfn "${NDK_DIR}/sources/cxx-stl/llvm-libc++abi" \
            "${NDK_DIR}/toolchain/llvm-project/libcxxabi"
fi

NDK_MODULE_PATH="${NDK_DIR}" \
"${NDK_DIR}/ndk-build" \
    NDK_PROJECT_PATH="${JNI_DIR}" \
    APP_BUILD_SCRIPT="${JNI_DIR}/Android.mk" \
    NDK_APPLICATION_MK="${JNI_DIR}/Application.mk" \
    -C "${JNI_DIR}" \
    -B \
    2>&1

# ── 3. Copy to jniLibs ───────────────────────────────────────────────────────
echo ""
echo -e "${YELLOW}[3/4] Copying library.so to jniLibs...${NC}"
JNILIBS_DIR="app/src/main/jniLibs/arm64-v8a"
mkdir -p "${JNILIBS_DIR}"
if [ -f "${OUT_SO}" ]; then
    cp "${OUT_SO}" "${JNILIBS_DIR}/library.so"
    echo -e "${GREEN}      Copied to ${JNILIBS_DIR}/library.so${NC}"
fi

# ── 4. Report ────────────────────────────────────────────────────────────────
echo ""
if [ -f "${OUT_SO}" ]; then
    SIZE=$(du -sh "${OUT_SO}" | cut -f1)
    MD5=$(md5sum "${OUT_SO}" | cut -d' ' -f1)
    echo -e "${GREEN}[4/4] BUILD SUCCESS ✓${NC}"
    echo -e "      File : ${OUT_SO}"
    echo -e "      Size : ${SIZE}"
    echo -e "      MD5  : ${MD5}"
    echo ""
    echo -e "${YELLOW}      → Flash library.so to your 8BP mod folder:${NC}"
    echo -e "         /data/app/com.miniclip.eightballpool-*/lib/arm64/"
else
    echo -e "${RED}[4/4] BUILD FAILED — library.so not found${NC}"
    exit 1
fi
