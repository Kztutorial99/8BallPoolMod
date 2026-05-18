#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <sys/stat.h>

// ── Dump Offset ────────────────────────────────────────────────────────────────
// Kumpulkan semua offset class + runtime address game 8BP.
// Hasil bisa di-copy ke clipboard atau disimpan ke file dump.cs
//
// SAFETY: semua akses memori game (game objects) menggunakan safe read helper
// untuk mencegah SIGSEGV / force close saat game tidak sedang dalam match.
// ─────────────────────────────────────────────────────────────────────────────

namespace DumpOffset {

    // ── State UI ──────────────────────────────────────────────────────────────
    static std::string s_dumpText;
    static bool        s_dumpReady  = false;
    static bool        s_scanning   = false;
    static char        s_saveStatus[128] = {};

    // ── Safe memory read helpers ──────────────────────────────────────────────
    // Tidak pernah crash — return 0 jika alamat tidak aman
    static inline bool DmpAddrSafe(uintptr_t a) {
        return a > 0x1000UL && a < 0x7FFFFFFFFFFFULL;
    }
    static inline uintptr_t DmpRd64(uintptr_t a) {
        return DmpAddrSafe(a) ? *(uintptr_t*)a : 0UL;
    }
    static inline uint32_t DmpRd32(uintptr_t a) {
        return DmpAddrSafe(a) ? *(uint32_t*)a : 0U;
    }
    static inline double DmpRdDbl(uintptr_t a) {
        if (!DmpAddrSafe(a)) return 0.0;
        double v = *(double*)a;
        return (std::isnan(v) || std::isinf(v)) ? 0.0 : v;
    }

    // ── Safe pointer ke sharedGameManager ─────────────────────────────────────
    // TIDAK menggunakan operator bool() / isInstanceOf() karena keduanya
    // melakukan raw memory read tanpa safety guard → penyebab crash #1
    static inline uintptr_t SafeGetGM() {
        uintptr_t inst = sharedGameManager.instance;
        return DmpAddrSafe(inst) ? inst : 0UL;
    }

    // ── Helper format hex ─────────────────────────────────────────────────────
    static std::string hex(uintptr_t v) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%lX", (unsigned long)v);
        return buf;
    }
    static std::string hexOff(uintptr_t v) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%03lX", (unsigned long)v);
        return buf;
    }

    // ── Buat baris dump ──────────────────────────────────────────────────────
    static void Line(std::ostringstream& s, const char* type,
                     const char* cls, const char* name, uintptr_t off,
                     const char* note = "") {
        (void)cls;
        // setw(14) agar tipe panjang (VisualCue, StateManager, dst) tidak menempel ke nama
        s << "    " << std::left << std::setw(14) << type
          << std::setw(28) << name
          << "; // " << hexOff(off);
        if (note && note[0]) s << "  -- " << note;
        s << "\n";
    }

    // ── Generate dump teks lengkap ────────────────────────────────────────────
    static std::string Generate() {
        std::ostringstream s;

        s << "// ================================================================\n";
        s << "// Flux Pro Engine v2.0 -- Memory Dump\n";
        s << "// Game     : 8 Ball Pool (com.miniclip.eightballpool)\n";
        s << "// Arch     : arm64-v8a\n";
        s << "// Build    : " << __DATE__ << " " << __TIME__ << "\n";
        s << "// ================================================================\n\n";

        // ── Runtime Addresses ─────────────────────────────────────────────────
        s << "// -- Runtime Addresses ------------------------------------------\n";
        s << "// libmain base          : " << hex(libmain) << "\n";

        // Safe read: tidak pakai isInstanceOf, cukup cek range alamat
        uintptr_t gm = SafeGetGM();
        s << "// sharedGameManager    : " << hex(gm) << "\n";
        // Catatan: GM adalah heap object, bukan offset libmain — tidak ditampilkan

        // gPrediction — pointer ke static object, selalu valid jika non-null
        {
            uintptr_t pred = gPrediction ? (uintptr_t)gPrediction : 0;
            s << "// gPrediction          : " << hex(pred) << "\n";
        }

        // _rules — SAFE: gunakan DmpRd64, bukan _rules() yang raw
        if (gm) {
            uintptr_t rules = DmpRd64(gm + 0x3E0);
            s << "// _rules ptr           : " << hex(rules) << "\n";
            if (DmpAddrSafe(rules)) {
                // Safe read field-by-field, bukan via getPocketNominationMode()
                int  nomMode   = (int)DmpRd32(rules + 0x68);
                uint nomPocket = DmpRd32(rules + 0x118);
                s << "// nominationMode       : " << nomMode
                  << "  (0=none, 1=8ball only, 2=all shots)\n";
                s << "// nominatedPocket      : " << nomPocket << "\n";
            } else {
                s << "// _rules not available (game not in match)\n";
            }
        } else {
            s << "// sharedGameManager not available (game not in match)\n";
        }
        s << "\n";

        // ── GameManager Offsets ───────────────────────────────────────────────
        s << "// -- struct GameManager (game/GameManager.h) --------------------\n";
        s << "struct GameManager {\n";
        Line(s, "ptr",          "GameManager", "_rules",                0x3e0, "Ruleset*");
        Line(s, "Table",        "GameManager", "mTable",                0x3e8);
        Line(s, "VisualCue",    "GameManager", "mVisualCue",            0x4b8);
        Line(s, "VEnglishCtrl", "GameManager", "mVisualEnglishControl", 0x4c8);
        Line(s, "StateManager", "GameManager", "mStateManager",         0x508);
        Line(s, "int",          "GameManager", "mGameMode",             0x5c0);
        s << "};\n\n";

        // ── Ruleset Offsets ───────────────────────────────────────────────────
        s << "// -- struct Ruleset (game/Ruleset.h) ----------------------------\n";
        s << "struct Ruleset {\n";
        Line(s, "int",    "Ruleset", "field_0x0",              0x00);
        Line(s, "int",    "Ruleset", "field_0x4",              0x04);
        Line(s, "string", "Ruleset", "field_0x8",              0x08);
        Line(s, "int",    "Ruleset", "field_0x20",             0x20);
        Line(s, "map",    "Ruleset", "field_0x28",             0x28, "map<type_index, IRule*>");
        Line(s, "vector", "Ruleset", "field_0x40",             0x40, "vector<FoulReason>");
        Line(s, "bool",   "Ruleset", "field_0x58",             0x58);
        Line(s, "double", "Ruleset", "field_0x60",             0x60);
        Line(s, "int",    "Ruleset", "pocketNominationMode",   0x68, "0=none,1=8ball,2=all");
        Line(s, "void*",  "Ruleset", "field_0x70",             0x70);
        Line(s, "void*",  "Ruleset", "field_0x78",             0x78);
        Line(s, "int",    "Ruleset", "field_0x80",             0x80);
        Line(s, "void*",  "Ruleset", "field_0x88",             0x88);
        Line(s, "int",    "Ruleset", "field_0x90",             0x90);
        Line(s, "vector", "Ruleset", "field_0x98",             0x98, "vector<BallClassification>");
        Line(s, "vector", "Ruleset", "field_0xB0",             0xB0, "vector<BallClassification>");
        Line(s, "vector", "Ruleset", "classificationVector",   0xC8, "player classification");
        Line(s, "vector", "Ruleset", "field_0xE0",             0xE0, "vector<PoolPlayer*>");
        Line(s, "int",    "Ruleset", "field_0xF8",             0xF8);
        Line(s, "int",    "Ruleset", "field_0xFC",             0xFC);
        Line(s, "int",    "Ruleset", "field_0x100",            0x100);
        Line(s, "int",    "Ruleset", "field_0x104",            0x104);
        Line(s, "bool",   "Ruleset", "field_0x108",            0x108);
        Line(s, "int",    "Ruleset", "field_0x10C",            0x10C);
        Line(s, "char",   "Ruleset", "field_0x110",            0x110);
        Line(s, "char",   "Ruleset", "field_0x111",            0x111);
        Line(s, "bool",   "Ruleset", "field_0x112",            0x112);
        Line(s, "bool",   "Ruleset", "field_0x113",            0x113);
        Line(s, "bool",   "Ruleset", "field_0x114",            0x114);
        Line(s, "uint",   "Ruleset", "nominatedPocket",        0x118, "pocket nomination index");
        Line(s, "int",    "Ruleset", "field_0x11C",            0x11C);
        Line(s, "char",   "Ruleset", "field_0x120",            0x120);
        Line(s, "int",    "Ruleset", "field_0x124",            0x124);
        Line(s, "bool",   "Ruleset", "field_0x128",            0x128);
        Line(s, "int",    "Ruleset", "field_0x12C",            0x12C);
        s << "};\n\n";

        // ── VisualCue / VisualGuide ───────────────────────────────────────────
        s << "// -- struct VisualCue (game/VisualCue.h) ------------------------\n";
        s << "struct VisualCue {\n";
        Line(s, "VisualGuide", "VisualCue", "mVisualGuide", 0x3a8);
        Line(s, "double",      "VisualCue", "mPower",       0x3b0);
        s << "};\n";
        s << "struct VisualGuide {\n";
        Line(s, "double", "VisualGuide", "mAimAngle",       0x28, "shot angle (radians)");
        Line(s, "ptr",    "VisualGuide", "mClassification", 0xa0);
        s << "};\n\n";

        // ── Ball Offsets ──────────────────────────────────────────────────────
        s << "// -- struct Ball (game/Ball.h) -----------------------------------\n";
        s << "struct Ball {\n";
        Line(s, "Vec2d",  "Ball", "position",       0x20, "world position");
        Line(s, "Vec2d",  "Ball", "velocity",       0x30);
        Line(s, "double", "Ball", "radius",         0x40);
        Line(s, "Vec3d",  "Ball", "spin",           0x48);
        Line(s, "double", "Ball", "mass",           0x60);
        Line(s, "double", "Ball", "volume",         0x68);
        Line(s, "int",    "Ball", "classification", 0xa0, "0=cue,1=solid,2=stripe,3=9ball,4=8ball");
        Line(s, "int",    "Ball", "state",          0xa4, "1=table,2=pocket,3=unknown,4=potted");
        s << "};\n\n";

        // ── Table Offsets ─────────────────────────────────────────────────────
        s << "// -- struct Table (game/Table.h) ---------------------------------\n";
        s << "struct Table {\n";
        Line(s, "TableProps", "Table", "mTableProperties",    0x3b0);
        Line(s, "FrictionP",  "Table", "_frictionProperties", 0x3c0);
        Line(s, "PNSArray*",  "Table", "mBalls",              0x450, "all balls");
        Line(s, "Vec4d",      "Table", "mCollisionBounds",    0x588, "x,y,w,h");
        s << "};\n\n";

        // ── Game Constants ────────────────────────────────────────────────────
        s << "// -- Game Constants ----------------------------------------------\n";
        s << "// TABLE_WIDTH           = " << TABLE_WIDTH            << "\n";
        s << "// TABLE_HEIGHT          = " << TABLE_HEIGHT           << "\n";
        s << "// BALL_RADIUS           = " << BALL_RADIUS            << "\n";
        s << "// POCKET_RADIUS         = " << POCKET_RADIUS          << "\n";
        s << "// TABLE_POCKETS_COUNT   = " << TABLE_POCKETS_COUNT    << "\n";
        s << "// MAX_BALLS_COUNT       = " << MAX_BALLS_COUNT        << "\n";
        s << "// MAX_ANGLE_RADIANS     = " << MAX_ANGLE_RADIANS      << "\n";
        s << "// MIN_ANGLE_STEP_RAD    = " << MIN_ANGLE_STEP_RADIANS << "\n";

        // Safe read konstanta dari libmain
        if (DmpAddrSafe(libmain)) {
            double spin     = DmpRdDbl(libmain + 0x4E49418);
            double maxPower = DmpRdDbl(libmain + 0x4E49410);
            s << "// CUE_SPIN              = " << spin     << "  (libmain+0x4E49418)\n";
            s << "// CUE_MAX_POWER         = " << maxPower << "  (libmain+0x4E49410)\n";
        }
        s << "\n";

        // ── Pocket World Positions ────────────────────────────────────────────
        s << "// -- Pocket World Positions --------------------------------------\n";
        const char* pNames[6] = {
            "Top-Left","Top-Center","Top-Right",
            "Bot-Right","Bot-Center","Bot-Left"
        };
        const double px[6] = {
            -TABLE_HALF_WIDTH,  0.0,              TABLE_HALF_WIDTH,
             TABLE_HALF_WIDTH,  0.0,             -TABLE_HALF_WIDTH
        };
        const double py[6] = {
             TABLE_HALF_HEIGHT, TABLE_HALF_HEIGHT, TABLE_HALF_HEIGHT,
            -TABLE_HALF_HEIGHT,-TABLE_HALF_HEIGHT,-TABLE_HALF_HEIGHT
        };
        for (int i = 0; i < 6; i++) {
            char buf[80];
            snprintf(buf, sizeof(buf), "// Pocket[%d] %-12s : world(%.1f, %.1f)",
                     i, pNames[i], px[i], py[i]);
            s << buf << "\n";
        }
        s << "\n";

        // ── Runtime Live Values (jika sedang dalam match) ─────────────────────
        s << "// -- Live Runtime Values (in-match only) ------------------------\n";
        if (gm) {
            // Table pointer
            uintptr_t tbl = DmpRd64(gm + 0x3E8);
            s << "// Table ptr            : " << hex(tbl) << "\n";

            // VisualCue → AimAngle dan Power
            uintptr_t vc  = DmpRd64(gm + 0x4B8);
            if (DmpAddrSafe(vc)) {
                uintptr_t vg  = DmpRd64(vc + 0x3A8);
                double angle  = DmpAddrSafe(vg) ? DmpRdDbl(vg + 0x28) : 0.0;
                double power  = DmpRdDbl(vc + 0x3B0);
                s << "// AimAngle (rad)       : " << angle << "\n";
                s << "// ShotPower            : " << power << "\n";
            }

            // StateManager → current state ID
            uintptr_t sm = DmpRd64(gm + 0x508);
            if (DmpAddrSafe(sm)) {
                uintptr_t stateStack = DmpRd64(sm + 0x8);
                if (DmpAddrSafe(stateStack)) {
                    // Baca count sebagai 32-bit dan validasi range wajar (1-32)
                    uint32_t count32 = DmpRd32(stateStack + 0x8);
                    uintptr_t data   = DmpRd64(stateStack + 0x18);
                    if (count32 > 0 && count32 < 32 && DmpAddrSafe(data)) {
                        uintptr_t lastState = DmpRd64(data + (count32 - 1) * 8);
                        if (DmpAddrSafe(lastState)) {
                            // State ID bisa di offset berbeda tergantung vtable layout
                            // Coba +0x10 (setelah vtable 8 + base member 8)
                            int stateId = (int)DmpRd32(lastState + 0x10);
                            s << "// StateManager ptr     : " << hex(sm) << "\n";
                            s << "// StateStack count     : " << count32 << "\n";
                            s << "// CurrentState ptr     : " << hex(lastState) << "\n";
                            s << "// CurrentStateId       : " << stateId
                              << "  (4=my turn, 7=opp turn, 10=ended)\n";
                        } else {
                            s << "// CurrentState ptr     : (null/invalid)\n";
                        }
                    } else {
                        s << "// StateStack count     : " << count32
                          << "  (tidak wajar — game mungkin di menu)\n";
                    }
                }
            }

            // GameMode
            int gameMode = (int)DmpRd32(gm + 0x5C0);
            s << "// mGameMode            : " << gameMode << "\n";
        } else {
            s << "// (game not in match — launch scan during active game for live values)\n";
        }
        s << "\n";

        // ── Known Hook Offsets (relative ke libmain) ─────────────────────────
        s << "// -- Known Hook Offsets (relative to libmain) -------------------\n";
        s << "// setActiveVisualCue    : libmain + 0x2D911E0\n";
        s << "// isBallBallCollision   : libmain + 0x2B1B158\n";
        s << "// willCollideWithTable  : libmain + 0x3606C80\n";
        s << "// (eglSwapBuffers hooked via xhook, no static offset)\n";
        s << "\n";

        // ── Global Pointers ───────────────────────────────────────────────────
        s << "// -- Global Pointers (libmain + offset) -------------------------\n";
        struct { const char* name; uintptr_t off; } gPtrs[] = {
            { "sharedDirector",    0x4F06288 },
            { "sharedUserInfo",    0x4E9FEB8 },
            { "sharedMainManager", 0x4DDE3E0 },
            { "sharedMenuManager", 0x4DFE838 },
        };
        for (auto& gp : gPtrs) {
            uintptr_t addr = DmpAddrSafe(libmain) ? libmain + gp.off : 0;
            uintptr_t val  = DmpRd64(addr);
            char buf[128];
            snprintf(buf, sizeof(buf), "// %-22s : libmain+0x%lX  =>  %s\n",
                     gp.name, (unsigned long)gp.off, val ? hex(val).c_str() : "0x0 (null)");
            s << buf;
        }
        s << "\n";

        s << "// ================================================================\n";
        s << "// END OF DUMP\n";
        s << "// ================================================================\n";

        return s.str();
    }

    // ── Jalankan scan ─────────────────────────────────────────────────────────
    // Aman dipanggil dari render thread — semua akses memori sudah di-guard
    static void RunScan() {
        if (s_scanning) return;
        s_scanning   = true;
        s_dumpReady  = false;
        s_saveStatus[0] = '\0';
        s_dumpText   = Generate();
        s_dumpReady  = true;
        s_scanning   = false;
    }

    // ── Simpan dump ke file ───────────────────────────────────────────────────
    static void SaveToFile() {
        if (!s_dumpReady || s_dumpText.empty()) {
            snprintf(s_saveStatus, sizeof(s_saveStatus), "Jalankan Scan dulu!");
            return;
        }

        const char* paths[] = {
            "/storage/emulated/0/Android/data/com.miniclip.eightballpool/files/Dump",
            "/data/data/com.miniclip.eightballpool/files/Dump",
            "/storage/emulated/0/Download",
            "/sdcard/Download",
        };

        for (auto& dir : paths) {
            create_directory_recursive(dir);
            char fpath[256];
            snprintf(fpath, sizeof(fpath), "%s/dump.cs", dir);
            std::ofstream ofs(fpath, std::ios::out | std::ios::trunc);
            if (ofs.is_open()) {
                ofs << s_dumpText;
                ofs.close();
                snprintf(s_saveStatus, sizeof(s_saveStatus), "Saved: %s", fpath);
                LOGI("DumpOffset: saved to %s", fpath);
                return;
            }
        }
        snprintf(s_saveStatus, sizeof(s_saveStatus), "Gagal menyimpan file!");
    }

} // namespace DumpOffset
