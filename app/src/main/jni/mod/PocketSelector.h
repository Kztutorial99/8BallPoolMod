#pragma once

#include <atomic>

// ── Pocket Selector ────────────────────────────────────────────────────────────
// User tap pocket di layar → aim otomatis lock ke pocket itu.
//
// Cara kerja:
//   1. nativeTouchesBegin memanggil PocketSelector::OnTouch(x, y)
//   2. Jika (x,y) dalam radius pocket, simpan index pocket ke selectedPocket
//   3. AimLockTarget::Aim() / AimLock8Ball::Aim() cek selectedPocket
//      - Jika >= 0 : hanya cari angle yang masukkan bola ke pocket itu
//      - Jika == -1: pilih pocket terbaik otomatis (default)
//
// Pocket index game 8BP:
//   0=Top-Left  1=Top-Center  2=Top-Right
//   3=Bot-Right 4=Bot-Center  5=Bot-Left
// ─────────────────────────────────────────────────────────────────────────────

namespace PocketSelector {
    // Pocket yang dipilih user (-1 = auto / tidak ada)
    static std::atomic<int> selectedPocket{-1};

    // Posisi world masing-masing pocket (6 pocket)
    static const Vec2d POCKET_WORLD[TABLE_POCKETS_COUNT] = {
        {-TABLE_HALF_WIDTH,  TABLE_HALF_HEIGHT},  // 0: Top-Left
        { 0.0,               TABLE_HALF_HEIGHT},  // 1: Top-Center
        { TABLE_HALF_WIDTH,  TABLE_HALF_HEIGHT},  // 2: Top-Right
        { TABLE_HALF_WIDTH, -TABLE_HALF_HEIGHT},  // 3: Bot-Right
        { 0.0,              -TABLE_HALF_HEIGHT},  // 4: Bot-Center
        {-TABLE_HALF_WIDTH, -TABLE_HALF_HEIGHT},  // 5: Bot-Left
    };

    static const char* POCKET_NAME[TABLE_POCKETS_COUNT] = {
        "TL", "TC", "TR", "BR", "BC", "BL"
    };

    // Radius deteksi tap dalam pixel — cukup besar untuk pocket icon game
    static constexpr float HIT_RADIUS_PX = 60.0f;

    // Cek apakah (tx, ty) mengenai pocket ke-idx
    static bool HitTestPocket(int idx, float tx, float ty) {
        ImVec2 ps = WorldToScreen(POCKET_WORLD[idx]);
        float dx = tx - ps.x;
        float dy = ty - ps.y;
        return (dx * dx + dy * dy) <= (HIT_RADIUS_PX * HIT_RADIUS_PX);
    }

    // Dipanggil dari nativeTouchesBegin (input thread) — cek apakah tap kena pocket
    // Return: pocket index yang kena, atau -1 jika tidak ada
    int OnTouch(float x, float y) {
        // Hanya aktif jika game sedang berjalan
        if (!sharedGameManager) return -1;

        for (int i = 0; i < TABLE_POCKETS_COUNT; i++) {
            if (HitTestPocket(i, x, y)) {
                selectedPocket.store(i);
                LOGI("PocketSelector: selected pocket %d (%s)", i, POCKET_NAME[i]);
                return i;
            }
        }
        return -1;
    }

    // Dipanggil dari GL render thread setiap frame — proses pending touch dari input thread
    void Update() {
        if (!g_pendingOutsideTouch) return;
        g_pendingOutsideTouch = false;
        float x = g_lastOutsideTouchX;
        float y = g_lastOutsideTouchY;
        if (!sharedGameManager) return;
        for (int i = 0; i < TABLE_POCKETS_COUNT; i++) {
            if (HitTestPocket(i, x, y)) {
                selectedPocket.store(i);
                LOGI("PocketSelector: pocket %d (%s) selected", i, POCKET_NAME[i]);
                return;
            }
        }
    }

    // Reset ke mode auto
    void Reset() {
        selectedPocket.store(-1);
    }

    // Ambil pocket yang dipilih (thread-safe)
    int Get() {
        return selectedPocket.load();
    }
}
