#pragma once

#include <atomic>

// ── Auto Pocket Nomination ─────────────────────────────────────────────────────
// Panggil nominatePocket() dari GL render thread setiap frame selagi aimed.
//
// Mengapa TIDAK menggunakan background thread touch injection:
//   - NativeTouchesBegin() memanggil _nativeTouchesBegin(env, nullptr, ...)
//   - Game (Cocos2d-x) TIDAK thread-safe untuk input dari C++ background thread
//   - nullptr jobject menyebabkan crash jika game mengakses obj parameter
//   - Solusinya: panggil nominatePocket() dari GL thread setiap frame (aman)
//
// Pocket index 8 Ball Pool:
//   0=Top-Left  1=Top-Center  2=Top-Right
//   3=Bot-Right 4=Bot-Center  5=Bot-Left
// ─────────────────────────────────────────────────────────────────────────────

namespace InjectTapPocket {
    bool bEnabled = false;

    // Pocket yang sedang dijadwalkan untuk dinominasikan (-1 = tidak ada)
    static std::atomic<int> s_nominatedPocket{-1};

    // Jadwalkan nominasi pocket — aman dipanggil dari thread manapun
    void TapPocket(int pocketIdx, int /*unused_delay*/ = 0) {
        if (!bEnabled) return;
        if (pocketIdx < 0 || pocketIdx >= TABLE_POCKETS_COUNT) return;
        s_nominatedPocket.store(pocketIdx);
    }

    // Reset state nominasi (saat giliran selesai / aim direset)
    void Reset() {
        s_nominatedPocket.store(-1);
    }

    // Dipanggil dari GL render thread setiap frame (di DrawMenu)
    // Pertahankan nominasi setiap frame agar game tidak bisa overwrite
    void Update() {
        if (!bEnabled) return;
        int idx = s_nominatedPocket.load();
        if (idx < 0) return;
        if (!sharedGameManager) return;

        sharedGameManager.nominatePocket(idx);
    }
}
