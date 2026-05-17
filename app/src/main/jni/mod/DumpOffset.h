#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <sys/stat.h>

// ── Dump Offset ────────────────────────────────────────────────────────────────
// Kumpulkan semua offset class + runtime address game 8BP.
// Hasil bisa di-copy ke clipboard atau disimpan ke file Dump.cs
// ─────────────────────────────────────────────────────────────────────────────

namespace DumpOffset {

    // ── State UI ──────────────────────────────────────────────────────────────
    static std::string s_dumpText;
    static bool        s_dumpReady  = false;
    static bool        s_scanning   = false;
    static char        s_saveStatus[128] = {};

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
        s << "    " << std::left << std::setw(8) << type
          << std::setw(28) << name
          << "; // " << hexOff(off);
        if (note && note[0]) s << "  — " << note;
        s << "\n";
    }

    // ── Scan class name di rentang memori ─────────────────────────────────────
    // Cari string ASCII printable yg terlihat seperti class name C++ di libmain
    static void ScanClassNames(std::ostringstream& s, uintptr_t base, size_t range) {
        s << "\n// ── Runtime Class Names (vtable+0x10 scan) ──────────────────────\n";
        int found = 0;
        const int MAX_FOUND = 80;
        // Scan kelipatan pointer (8 byte aligned)
        for (uintptr_t addr = base; addr < base + range - 0x20 && found < MAX_FOUND; addr += 8) {
            uintptr_t* vtable = (uintptr_t*)addr;
            uintptr_t vtPtr = *vtable;
            if (!vtPtr || vtPtr < base || vtPtr >= base + range) continue;
            // Baca pointer di vtable+0x10
            uintptr_t* nameSlot = (uintptr_t*)(vtPtr + 0x10);
            // Safety: cek apakah nameSlot bisa dibaca
            char* str = nullptr;
            if ((uintptr_t)nameSlot >= base && (uintptr_t)nameSlot < base + range) {
                str = (char*)*nameSlot;
            }
            if (!str) continue;
            // Validasi: ASCII printable, panjang 3-40
            bool ok = true;
            int len = 0;
            while (len < 42) {
                char c = str[len];
                if (c == 0) break;
                if (c < 0x20 || c > 0x7E) { ok = false; break; }
                len++;
            }
            if (!ok || len < 3 || len > 40) continue;
            s << "// [" << hex(addr - base) << "] " << str << "\n";
            found++;
        }
        if (found == 0) s << "// (tidak ada class name yang ditemukan)\n";
    }

    // ── Generate dump teks lengkap ────────────────────────────────────────────
    static std::string Generate() {
        std::ostringstream s;

        s << "// ================================================================\n";
        s << "// Flux Pro Engine v2.0 — Memory Dump\n";
        s << "// Game     : 8 Ball Pool (com.miniclip.eightballpool)\n";
        s << "// Arch     : arm64-v8a\n";
        s << "// Build    : " << __DATE__ << " " << __TIME__ << "\n";
        s << "// ================================================================\n\n";

        // ── Runtime Addresses ─────────────────────────────────────────────────
        s << "// ── Runtime Addresses ──────────────────────────────────────────\n";
        s << "// libmain base          : " << hex(libmain) << "\n";
        {
            uintptr_t gm = sharedGameManager ? sharedGameManager.instance : 0;
            s << "// sharedGameManager    : " << hex(gm) << "\n";
            if (gm) {
                s << "// sharedGameManager-libmain : +" << hex(gm - libmain) << "\n";
            }
        }
        {
            uintptr_t pred = gPrediction ? (uintptr_t)gPrediction : 0;
            s << "// gPrediction          : " << hex(pred) << "\n";
        }
        // _rules pointer runtime
        if (sharedGameManager) {
            auto rules = sharedGameManager._rules();
            s << "// _rules()             : " << hex(rules) << "\n";
            if (rules) {
                int nomMode    = sharedGameManager.getPocketNominationMode();
                uint nomPocket = sharedGameManager.getNominatedPocket();
                s << "// nominationMode       : " << nomMode
                  << "  (0=none, 1=8ball only, 2=all shots)\n";
                s << "// nominatedPocket      : " << nomPocket << "\n";
            }
        }
        s << "\n";

        // ── GameManager Offsets ───────────────────────────────────────────────
        s << "// ── struct GameManager (game/GameManager.h) ───────────────────\n";
        s << "struct GameManager {\n";
        Line(s, "ptr",          "GameManager", "_rules",              0x3e0, "Ruleset*");
        Line(s, "Table",        "GameManager", "mTable",              0x3e8);
        Line(s, "VisualCue",    "GameManager", "mVisualCue",          0x4b8);
        Line(s, "VEnglishCtrl", "GameManager", "mVisualEnglishControl",0x4c8);
        Line(s, "StateManager", "GameManager", "mStateManager",       0x508);
        Line(s, "int",          "GameManager", "mGameMode",           0x5c0);
        s << "};\n\n";

        // ── Ruleset Offsets ───────────────────────────────────────────────────
        s << "// ── struct Ruleset (game/Ruleset.h) ────────────────────────────\n";
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
        s << "// ── struct VisualCue (game/VisualCue.h) ────────────────────────\n";
        s << "struct VisualCue {\n";
        Line(s, "VisualGuide", "VisualCue", "mVisualGuide", 0x3a8);
        Line(s, "double",      "VisualCue", "mPower",       0x3b0);
        s << "};\n\n";
        s << "struct VisualGuide {\n";
        Line(s, "double", "VisualGuide", "mAimAngle",      0x28, "shot angle (radians)");
        Line(s, "ptr",    "VisualGuide", "mClassification",0xa0);
        s << "};\n\n";

        // ── Ball Offsets ──────────────────────────────────────────────────────
        s << "// ── struct Ball (game/Ball.h) ───────────────────────────────────\n";
        s << "struct Ball {\n";
        Line(s, "Vec2d",  "Ball", "position",       0x20,  "world position");
        Line(s, "Vec2d",  "Ball", "velocity",       0x30);
        Line(s, "double", "Ball", "radius",         0x40);
        Line(s, "Vec3d",  "Ball", "spin",           0x48);
        Line(s, "double", "Ball", "mass",           0x60);
        Line(s, "double", "Ball", "volume",         0x68);
        Line(s, "int",    "Ball", "classification", 0xa0,  "0=cue,1=solid,2=stripe,3=9ball,4=8ball");
        Line(s, "int",    "Ball", "state",          0xa4,  "1=table,2=pocket,3=unknown,4=potted");
        s << "};\n\n";

        // ── Game Constants ────────────────────────────────────────────────────
        s << "// ── Game Constants ──────────────────────────────────────────────\n";
        s << "// TABLE_WIDTH           = " << TABLE_WIDTH   << "\n";
        s << "// TABLE_HEIGHT          = " << TABLE_HEIGHT  << "\n";
        s << "// BALL_RADIUS           = " << BALL_RADIUS   << "\n";
        s << "// POCKET_RADIUS         = " << POCKET_RADIUS << "\n";
        s << "// TABLE_POCKETS_COUNT   = " << TABLE_POCKETS_COUNT << "\n";
        s << "// MAX_BALLS_COUNT       = " << MAX_BALLS_COUNT << "\n";
        s << "// MAX_ANGLE_RADIANS     = " << MAX_ANGLE_RADIANS << "\n";
        s << "// MIN_ANGLE_STEP_RAD    = " << MIN_ANGLE_STEP_RADIANS << "\n";
        s << "\n";

        // ── Pocket World Positions ────────────────────────────────────────────
        s << "// ── Pocket World Positions ──────────────────────────────────────\n";
        const char* pNames[6] = {"Top-Left","Top-Center","Top-Right","Bot-Right","Bot-Center","Bot-Left"};
        Vec2d pPositions[6] = {
            {-TABLE_HALF_WIDTH,  TABLE_HALF_HEIGHT},
            { 0.0,               TABLE_HALF_HEIGHT},
            { TABLE_HALF_WIDTH,  TABLE_HALF_HEIGHT},
            { TABLE_HALF_WIDTH, -TABLE_HALF_HEIGHT},
            { 0.0,              -TABLE_HALF_HEIGHT},
            {-TABLE_HALF_WIDTH, -TABLE_HALF_HEIGHT},
        };
        for (int i = 0; i < 6; i++) {
            char buf[80];
            snprintf(buf, sizeof(buf), "// Pocket[%d] %-12s : world(%.1f, %.1f)",
                i, pNames[i], pPositions[i].x, pPositions[i].y);
            s << buf << "\n";
        }
        s << "\n";

        // ── Known Hook Offsets (relative ke libmain) ─────────────────────────
        s << "// ── Known Hook Offsets (relative ke libmain) ───────────────────\n";
        s << "// setActiveVisualCue    : libmain + 0x2D911E0\n";
        s << "// (eglSwapBuffers via xhook tidak punya offset statis)\n";
        s << "\n";

        s << "// ================================================================\n";
        s << "// END OF DUMP\n";
        s << "// ================================================================\n";

        return s.str();
    }

    // ── Jalankan scan (bisa dipanggil dari render thread) ─────────────────────
    static void RunScan() {
        s_scanning  = true;
        s_dumpReady = false;
        s_dumpText  = Generate();
        s_dumpReady = true;
        s_scanning  = false;
        s_saveStatus[0] = '\0';
    }

    // ── Simpan dump ke file ───────────────────────────────────────────────────
    static void SaveToFile() {
        if (!s_dumpReady || s_dumpText.empty()) {
            snprintf(s_saveStatus, sizeof(s_saveStatus), "Jalankan scan dulu!");
            return;
        }

        // Coba path app-data dulu (tidak butuh permission)
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
