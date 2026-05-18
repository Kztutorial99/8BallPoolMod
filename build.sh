#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# LYN4XP / Flux Pro Engine — Full Build Script
# 1) Setup Android NDK (download jika perlu)
# 2) Setup Android SDK (download jika perlu)
# 3) Compile library.so (arm64-v8a) via ndk-build
# 4) Copy ke jniLibs
# 5) Build APK via Gradle
# ─────────────────────────────────────────────────────────────────────────────

set -e

# ── Konfigurasi ───────────────────────────────────────────────────────────────
NDK_VERSION="r25c"
NDK_DIR="/tmp/android-ndk-${NDK_VERSION}"
NDK_ZIP="/tmp/ndk.zip"
NDK_URL="https://dl.google.com/android/repository/android-ndk-${NDK_VERSION}-linux.zip"

SDK_DIR="${HOME}/workspace/.android-sdk"
SDK_TOOLS_ZIP="/tmp/cmdtools.zip"
SDK_TOOLS_URL="https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip"
SDKMANAGER="${SDK_DIR}/cmdline-tools/latest/bin/sdkmanager"

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
JNI_DIR="${ROOT_DIR}/app/src/main/jni"
OUT_DIR="${JNI_DIR}/libs/arm64-v8a"
OUT_SO="${OUT_DIR}/library.so"
OBJ_DIR="${JNI_DIR}/obj"
JNILIBS_DIR="${ROOT_DIR}/app/src/main/jniLibs/arm64-v8a"
LOCAL_PROP="${ROOT_DIR}/local.properties"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${YELLOW}╔══════════════════════════════════════════╗${NC}"
echo -e "${YELLOW}║   LYN4XP — Flux Pro Engine Build         ║${NC}"
echo -e "${YELLOW}║   8 Ball Pool Mod  •  arm64-v8a          ║${NC}"
echo -e "${YELLOW}╚══════════════════════════════════════════╝${NC}"
echo ""

# ── STEP 0: Clean old JNI build cache ────────────────────────────────────────
echo -e "${CYAN}[0/5] Cleaning old build cache...${NC}"
rm -rf "${OUT_DIR}" "${OBJ_DIR}"
mkdir -p "${OUT_DIR}"
echo -e "${GREEN}      Done${NC}"
echo ""

# ── STEP 1: Setup Android NDK ────────────────────────────────────────────────
echo -e "${CYAN}[1/5] Checking Android NDK ${NDK_VERSION}...${NC}"
if [ ! -d "$NDK_DIR" ]; then
    if [ ! -f "$NDK_ZIP" ]; then
        echo -e "${YELLOW}      Downloading NDK (${NDK_VERSION})...${NC}"
        curl -L --progress-bar -o "$NDK_ZIP" "$NDK_URL"
    fi
    echo -e "${YELLOW}      Extracting NDK...${NC}"
    unzip -q "$NDK_ZIP" -d /tmp/
    echo -e "${GREEN}      NDK ready at ${NDK_DIR}${NC}"
else
    echo -e "${GREEN}      NDK already present — skipping${NC}"
fi

# Fix llvm-project symlink untuk c++_static
if [ ! -d "${NDK_DIR}/toolchain/llvm-project/libcxxabi" ]; then
    mkdir -p "${NDK_DIR}/toolchain/llvm-project"
    ln -sfn "${NDK_DIR}/sources/cxx-stl/llvm-libc++abi" \
            "${NDK_DIR}/toolchain/llvm-project/libcxxabi"
fi
echo ""

# ── STEP 2: Setup Android SDK ────────────────────────────────────────────────
echo -e "${CYAN}[2/5] Checking Android SDK...${NC}"

if [ ! -f "$SDKMANAGER" ]; then
    echo -e "${YELLOW}      Downloading SDK command line tools...${NC}"
    mkdir -p "${SDK_DIR}"
    curl -L --progress-bar -o "$SDK_TOOLS_ZIP" "$SDK_TOOLS_URL"
    mkdir -p /tmp/cmdtools_extract
    unzip -q "$SDK_TOOLS_ZIP" -d /tmp/cmdtools_extract/
    mkdir -p "${SDK_DIR}/cmdline-tools/latest"
    mv /tmp/cmdtools_extract/cmdline-tools/* "${SDK_DIR}/cmdline-tools/latest/"
    rm -rf /tmp/cmdtools_extract
    echo -e "${GREEN}      SDK command tools installed${NC}"
fi

# Install build-tools dan platform jika belum ada
export ANDROID_HOME="$SDK_DIR"
if [ ! -d "${SDK_DIR}/build-tools/33.0.2" ] || [ ! -d "${SDK_DIR}/platforms/android-33" ]; then
    echo -e "${YELLOW}      Installing build-tools;33.0.2 + platforms;android-33...${NC}"
    yes | "$SDKMANAGER" --licenses > /dev/null 2>&1 || true
    "$SDKMANAGER" "build-tools;33.0.2" "platforms;android-33" 2>&1 | grep -v "^\[=" || true
    echo -e "${GREEN}      SDK components installed${NC}"
else
    echo -e "${GREEN}      SDK components already present — skipping${NC}"
fi

# Buat local.properties
echo "sdk.dir=${SDK_DIR}" > "$LOCAL_PROP"
echo -e "${GREEN}      local.properties updated: sdk.dir=${SDK_DIR}${NC}"
echo ""

# ── STEP 3: Compile library.so dengan ndk-build ───────────────────────────────
echo -e "${CYAN}[3/5] Compiling library.so (fresh build)...${NC}"
echo ""

NDK_MODULE_PATH="${NDK_DIR}" \
"${NDK_DIR}/ndk-build" \
    NDK_PROJECT_PATH="${JNI_DIR}" \
    APP_BUILD_SCRIPT="${JNI_DIR}/Android.mk" \
    NDK_APPLICATION_MK="${JNI_DIR}/Application.mk" \
    -C "${JNI_DIR}" \
    -B \
    -j4 \
    2>&1

if [ ! -f "${OUT_SO}" ]; then
    echo -e "${RED}[3/5] FAILED — library.so tidak ditemukan setelah build!${NC}"
    exit 1
fi

SIZE=$(du -sh "${OUT_SO}" | cut -f1)
MD5=$(md5sum "${OUT_SO}" | cut -d' ' -f1)
echo ""
echo -e "${GREEN}      library.so OK  •  Size: ${SIZE}  •  MD5: ${MD5}${NC}"
echo ""

# ── STEP 4: Copy ke jniLibs ──────────────────────────────────────────────────
echo -e "${CYAN}[4/5] Copying library.so ke jniLibs...${NC}"
mkdir -p "${JNILIBS_DIR}"
cp "${OUT_SO}" "${JNILIBS_DIR}/library.so"
echo -e "${GREEN}      Copied → ${JNILIBS_DIR}/library.so${NC}"
echo ""

# ── STEP 5: Build APK dengan Gradle ──────────────────────────────────────────
echo -e "${CYAN}[5/5] Building APK dengan Gradle...${NC}"
chmod +x "${ROOT_DIR}/gradlew"

cd "${ROOT_DIR}"
"${ROOT_DIR}/gradlew" assembleDebug \
    -PANDROID_HOME="${SDK_DIR}" \
    2>&1 | grep -E "BUILD|FAILED|error:|warning:|Task :|Exception" || true

APK_PATH="${ROOT_DIR}/app/build/outputs/apk/debug/app-debug.apk"

echo ""
if [ -f "$APK_PATH" ]; then
    APK_SIZE=$(du -sh "$APK_PATH" | cut -f1)
    APK_MD5=$(md5sum "$APK_PATH" | cut -d' ' -f1)
    echo -e "${GREEN}╔══════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║   BUILD SUCCESS ✓                        ║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "  library.so : ${OUT_SO}"
    echo -e "  Size       : ${SIZE}  •  MD5: ${MD5}"
    echo ""
    echo -e "  APK        : ${APK_PATH}"
    echo -e "  Size       : ${APK_SIZE}  •  MD5: ${APK_MD5}"
    echo ""
    echo -e "${YELLOW}  Install ke device:${NC}"
    echo -e "    adb install -r ${APK_PATH}"
    echo ""
    echo -e "${YELLOW}  Atau flash library.so langsung:${NC}"
    echo -e "    /data/app/com.miniclip.eightballpool-*/lib/arm64/library.so"
else
    echo -e "${RED}╔══════════════════════════════════════════╗${NC}"
    echo -e "${RED}║   BUILD FAILED — APK tidak ditemukan     ║${NC}"
    echo -e "${RED}╚══════════════════════════════════════════╝${NC}"
    exit 1
fi
