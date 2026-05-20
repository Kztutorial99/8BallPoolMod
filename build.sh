#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# LYN4XP / Flux Pro Engine — Full Build Script
# 1) Setup Android NDK  (download jika perlu)
# 2) Setup Android SDK  (download jika perlu)
# 3) Compile library.so (arm64-v8a) via ndk-build
# 4) Copy library.so ke jniLibs
# 5) Build APK via Gradle
# ─────────────────────────────────────────────────────────────────────────────

set -eo pipefail

# ── Konfigurasi ───────────────────────────────────────────────────────────────
GRADLE_WRAPPER_FILE="$(dirname "$0")/gradle/wrapper/gradle-wrapper.properties"
GRADLE_MIN_VERSION="7.6.4"   # minimum versi yang support Java 19+
NDK_VERSION="r25c"
NDK_DIR="/tmp/android-ndk-${NDK_VERSION}"
NDK_ZIP="/tmp/ndk-${NDK_VERSION}.zip"
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
LOG_FILE="${ROOT_DIR}/build.log"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# ── Helper ────────────────────────────────────────────────────────────────────
ok()   { echo -e "${GREEN}  ✓ $*${NC}"; }
info() { echo -e "${CYAN}  → $*${NC}"; }
warn() { echo -e "${YELLOW}  ! $*${NC}"; }
fail() { echo -e "${RED}  ✗ $*${NC}"; exit 1; }

echo -e "${YELLOW}${BOLD}"
echo "╔════════════════════════════════════════════╗"
echo "║   LYN4XP — Flux Pro Engine Build           ║"
echo "║   8 Ball Pool Mod  •  arm64-v8a            ║"
echo "╚════════════════════════════════════════════╝"
echo -e "${NC}"
echo "  Log: ${LOG_FILE}"
echo ""

# ── STEP 0a: Auto-upgrade Gradle wrapper jika versi terlalu lama ─────────────
# Gradle <7.6 tidak support Java 19 — auto-fix agar build tidak gagal
if [ -f "${GRADLE_WRAPPER_FILE}" ]; then
    CUR_GRADLE=$(grep distributionUrl "${GRADLE_WRAPPER_FILE}" | grep -oP '(?<=gradle-)[0-9]+\.[0-9]+(\.[0-9]+)?' || echo "unknown")
    # Bandingkan versi: jika < 7.6.4, upgrade
    NEED_UPGRADE=false
    IFS='.' read -r cur_maj cur_min cur_pat <<< "${CUR_GRADLE}"
    IFS='.' read -r min_maj min_min min_pat <<< "${GRADLE_MIN_VERSION}"
    cur_pat=${cur_pat:-0}; min_pat=${min_pat:-0}
    if [ "${cur_maj:-0}" -lt "${min_maj}" ] || \
       ([ "${cur_maj:-0}" -eq "${min_maj}" ] && [ "${cur_min:-0}" -lt "${min_min}" ]) || \
       ([ "${cur_maj:-0}" -eq "${min_maj}" ] && [ "${cur_min:-0}" -eq "${min_min}" ] && [ "${cur_pat:-0}" -lt "${min_pat}" ]); then
        NEED_UPGRADE=true
    fi
    if [ "${NEED_UPGRADE}" = "true" ]; then
        warn "Gradle ${CUR_GRADLE} tidak support Java 19 — upgrade ke ${GRADLE_MIN_VERSION}..."
        sed -i "s|gradle-[0-9]*\.[0-9]*\(\.[0-9]*\)*-bin\.zip|gradle-${GRADLE_MIN_VERSION}-bin.zip|g" \
            "${GRADLE_WRAPPER_FILE}"
        ok "Gradle wrapper → ${GRADLE_MIN_VERSION}"
    else
        ok "Gradle wrapper OK (${CUR_GRADLE})"
    fi
fi

# ── STEP 0b: Clean old JNI build cache ───────────────────────────────────────
echo -e "${CYAN}[0/5] Cleaning old build cache...${NC}"
rm -rf "${OUT_DIR}" "${OBJ_DIR}"
mkdir -p "${OUT_DIR}"
ok "Done"
echo ""

# ── STEP 1: Setup Android NDK ────────────────────────────────────────────────
echo -e "${CYAN}[1/5] Checking Android NDK ${NDK_VERSION}...${NC}"
if [ ! -d "${NDK_DIR}" ]; then
    if [ ! -f "${NDK_ZIP}" ]; then
        info "Downloading NDK ${NDK_VERSION}..."
        curl -L --progress-bar -o "${NDK_ZIP}" "${NDK_URL}" \
            || fail "Gagal download NDK. Cek koneksi internet."
    fi

    # Validasi zip sebelum extract
    if ! unzip -t "${NDK_ZIP}" > /dev/null 2>&1; then
        warn "File zip NDK rusak — menghapus dan download ulang..."
        rm -f "${NDK_ZIP}"
        curl -L --progress-bar -o "${NDK_ZIP}" "${NDK_URL}" \
            || fail "Gagal download NDK (percobaan ke-2)."
    fi

    info "Extracting NDK ke /tmp/..."
    unzip -q "${NDK_ZIP}" -d /tmp/ || fail "Gagal extract NDK."
    ok "NDK ready di ${NDK_DIR}"
else
    ok "NDK sudah ada — skip download"
fi

# Fix symlink c++_static (diperlukan beberapa NDK build)
if [ ! -d "${NDK_DIR}/toolchain/llvm-project/libcxxabi" ]; then
    mkdir -p "${NDK_DIR}/toolchain/llvm-project"
    ln -sfn "${NDK_DIR}/sources/cxx-stl/llvm-libc++abi" \
            "${NDK_DIR}/toolchain/llvm-project/libcxxabi" 2>/dev/null || true
fi
echo ""

# ── STEP 2: Setup Android SDK ────────────────────────────────────────────────
echo -e "${CYAN}[2/5] Checking Android SDK...${NC}"

if [ ! -f "${SDKMANAGER}" ]; then
    info "Downloading SDK command line tools..."
    mkdir -p "${SDK_DIR}"
    curl -L --progress-bar -o "${SDK_TOOLS_ZIP}" "${SDK_TOOLS_URL}" \
        || fail "Gagal download SDK tools."
    mkdir -p /tmp/cmdtools_extract
    unzip -q "${SDK_TOOLS_ZIP}" -d /tmp/cmdtools_extract/ \
        || fail "Gagal extract SDK tools."
    mkdir -p "${SDK_DIR}/cmdline-tools/latest"
    mv /tmp/cmdtools_extract/cmdline-tools/* "${SDK_DIR}/cmdline-tools/latest/"
    rm -rf /tmp/cmdtools_extract "${SDK_TOOLS_ZIP}"
    ok "SDK tools installed"
else
    ok "SDK tools sudah ada — skip download"
fi

export ANDROID_HOME="${SDK_DIR}"
export ANDROID_SDK_ROOT="${SDK_DIR}"

if [ ! -d "${SDK_DIR}/build-tools/33.0.2" ] || [ ! -d "${SDK_DIR}/platforms/android-33" ]; then
    info "Installing build-tools;33.0.2 + platforms;android-33..."
    yes | "${SDKMANAGER}" --licenses > /dev/null 2>&1 || true
    "${SDKMANAGER}" "build-tools;33.0.2" "platforms;android-33" \
        2>&1 | grep -v "^\[=" || true
    ok "SDK components installed"
else
    ok "SDK components sudah ada — skip install"
fi

# Tulis local.properties
{
    echo "sdk.dir=${SDK_DIR}"
    echo "ndk.dir=${NDK_DIR}"
} > "${LOCAL_PROP}"
ok "local.properties updated"
echo ""

# ── STEP 3: Detect Java ───────────────────────────────────────────────────────
echo -e "${CYAN}[3/5] Detecting Java...${NC}"
if [ -z "${JAVA_HOME}" ]; then
    for candidate in \
        /usr/lib/jvm/java-17-openjdk-amd64 \
        /usr/lib/jvm/java-17-openjdk \
        /usr/lib/jvm/java-11-openjdk-amd64 \
        /usr/lib/jvm/java-11-openjdk \
        /usr/lib/jvm/java-8-openjdk-amd64 \
        /usr/lib/jvm/default-java \
        /usr/lib/jvm/java-17 \
        /usr/lib/jvm/java-11; do
        if [ -d "${candidate}" ]; then
            export JAVA_HOME="${candidate}"
            break
        fi
    done
    # Fallback: pakai PATH java (termasuk Nix/GraalVM store)
    if [ -z "${JAVA_HOME}" ] && command -v java &>/dev/null; then
        JAVA_BIN=$(readlink -f "$(command -v java)" 2>/dev/null || command -v java)
        export JAVA_HOME="${JAVA_BIN%/bin/java}"
    fi
fi

# Hapus Gradle cache jika ada versi Java yang tidak kompatibel
if [ -d "${HOME}/.gradle/caches" ]; then
    CACHE_ISSUES=$(find "${HOME}/.gradle/caches" -name "*.lock" 2>/dev/null | wc -l)
    if [ "${CACHE_ISSUES}" -gt 0 ]; then
        warn "Membersihkan Gradle cache lama untuk menghindari konflik versi Java..."
        rm -rf "${HOME}/.gradle/caches"
    fi
fi

if [ -n "${JAVA_HOME}" ]; then
    ok "JAVA_HOME = ${JAVA_HOME}"
    export PATH="${JAVA_HOME}/bin:${PATH}"
else
    fail "Java tidak ditemukan. Install JDK 17 atau 11 terlebih dahulu."
fi
echo ""

# ── STEP 4: Compile library.so via ndk-build ─────────────────────────────────
echo -e "${CYAN}[4/5] Compiling library.so (fresh build)...${NC}"
echo ""

JOBS=$(nproc 2>/dev/null || echo 4)
info "Menggunakan ${JOBS} core paralel"
echo ""

"${NDK_DIR}/ndk-build" \
    NDK_PROJECT_PATH="${JNI_DIR}" \
    APP_BUILD_SCRIPT="${JNI_DIR}/Android.mk" \
    NDK_APPLICATION_MK="${JNI_DIR}/Application.mk" \
    NDK_OUT="${OBJ_DIR}" \
    NDK_LIBS_OUT="${JNI_DIR}/libs" \
    APP_ABI="arm64-v8a" \
    -C "${JNI_DIR}" \
    -B \
    -j"${JOBS}" \
    2>&1 | tee -a "${LOG_FILE}"

if [ ! -f "${OUT_SO}" ]; then
    fail "library.so tidak ditemukan setelah build! Cek ${LOG_FILE}"
fi

SO_SIZE=$(du -sh "${OUT_SO}" | cut -f1)
SO_MD5=$(md5sum "${OUT_SO}" | cut -d' ' -f1)
ok "library.so OK  —  Size: ${SO_SIZE}  •  MD5: ${SO_MD5}"
echo ""

# ── STEP 5a: Copy library.so ke jniLibs ──────────────────────────────────────
echo -e "${CYAN}[5/5] Copying library.so & building APK...${NC}"
mkdir -p "${JNILIBS_DIR}"
cp "${OUT_SO}" "${JNILIBS_DIR}/library.so"
ok "library.so → ${JNILIBS_DIR}/library.so"

# Juga update libs/ dan obj/ di app/src/main (jika ada)
for DST in \
    "${ROOT_DIR}/app/src/main/libs/arm64-v8a" \
    "${ROOT_DIR}/app/src/main/obj/local/arm64-v8a"; do
    if [ -d "${DST}" ]; then
        cp "${OUT_SO}" "${DST}/library.so"
        ok "library.so → ${DST}/library.so"
    fi
done
echo ""

# ── STEP 5b: Build APK via Gradle ────────────────────────────────────────────
chmod +x "${ROOT_DIR}/gradlew"
cd "${ROOT_DIR}"

info "Running Gradle assembleDebug..."
"${ROOT_DIR}/gradlew" assembleDebug \
    --no-daemon \
    --warning-mode all \
    -PANDROID_HOME="${SDK_DIR}" \
    -Pandroid.ndkPath="${NDK_DIR}" \
    2>&1 | tee -a "${LOG_FILE}"

APK_PATH="${ROOT_DIR}/app/build/outputs/apk/debug/app-debug.apk"

echo ""
if [ -f "${APK_PATH}" ]; then
    APK_SIZE=$(du -sh "${APK_PATH}" | cut -f1)
    APK_MD5=$(md5sum "${APK_PATH}" | cut -d' ' -f1)

    # Copy APK ke root project untuk mudah diakses
    cp "${APK_PATH}" "${ROOT_DIR}/FluxPro-debug.apk"

    echo -e "${GREEN}${BOLD}"
    echo "╔════════════════════════════════════════════╗"
    echo "║   BUILD SUCCESS ✓                          ║"
    echo "╚════════════════════════════════════════════╝"
    echo -e "${NC}"
    echo -e "  ${CYAN}library.so${NC}"
    echo -e "    Path : ${OUT_SO}"
    echo -e "    Size : ${SO_SIZE}  •  MD5: ${SO_MD5}"
    echo ""
    echo -e "  ${CYAN}APK${NC}"
    echo -e "    Path : ${APK_PATH}"
    echo -e "    Copy : ${ROOT_DIR}/FluxPro-debug.apk"
    echo -e "    Size : ${APK_SIZE}  •  MD5: ${APK_MD5}"
    echo ""
    echo -e "  ${YELLOW}Install ke device via ADB:${NC}"
    echo -e "    adb install -r FluxPro-debug.apk"
    echo ""
    echo -e "  ${YELLOW}Flash library.so langsung (tanpa APK):${NC}"
    echo -e "    adb push ${JNILIBS_DIR}/library.so \\"
    echo -e "      /data/app/com.miniclip.eightballpool-*/lib/arm64/library.so"
    echo ""
    echo -e "  ${YELLOW}Log lengkap:${NC}  ${LOG_FILE}"
else
    echo -e "${RED}${BOLD}"
    echo "╔════════════════════════════════════════════╗"
    echo "║   BUILD FAILED — APK tidak ditemukan       ║"
    echo "╚════════════════════════════════════════════╝"
    echo -e "${NC}"
    echo -e "  ${YELLOW}Cek log lengkap:${NC}  ${LOG_FILE}"
    echo -e "  ${YELLOW}Atau jalankan ulang:${NC}  bash build.sh 2>&1 | tee build.log"
    exit 1
fi
