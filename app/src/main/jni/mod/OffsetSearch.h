#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>

// ── Offset Search ─────────────────────────────────────────────────────────────
// Database semua offset Data Class game 8 Ball Pool.
// Bisa dicari by nama class / field / type, hasil bisa disalin ke clipboard.
// ─────────────────────────────────────────────────────────────────────────────

namespace OffsetSearch {

    // ── Entry satu baris offset ───────────────────────────────────────────────
    struct Entry {
        const char* className;
        const char* fieldName;
        const char* type;
        uintptr_t   offset;
        const char* note;
    };

    // ── Database semua offset yang diketahui ──────────────────────────────────
    static const Entry s_database[] = {

        // ── GameManager ───────────────────────────────────────────────────────
        { "GameManager", "_rules",                  "ptr",          0x3E0, "Ruleset*" },
        { "GameManager", "mTable",                  "Table",        0x3E8, "" },
        { "GameManager", "mVisualCue",              "VisualCue",    0x4B8, "" },
        { "GameManager", "mVisualEnglishControl",   "VEnglishCtrl", 0x4C8, "" },
        { "GameManager", "mStateManager",           "StateManager", 0x508, "GameStateManager" },
        { "GameManager", "mGameMode",               "int",          0x5C0, "game mode id" },

        // ── Ruleset ───────────────────────────────────────────────────────────
        { "Ruleset", "field_0x0",              "int",    0x000, "" },
        { "Ruleset", "field_0x4",              "int",    0x004, "" },
        { "Ruleset", "field_0x8",              "string", 0x008, "" },
        { "Ruleset", "field_0x20",             "int",    0x020, "" },
        { "Ruleset", "field_0x28",             "map",    0x028, "map<type_index, IRule*>" },
        { "Ruleset", "field_0x40",             "vector", 0x040, "vector<FoulReason>" },
        { "Ruleset", "field_0x58",             "bool",   0x058, "" },
        { "Ruleset", "field_0x60",             "double", 0x060, "" },
        { "Ruleset", "pocketNominationMode",   "int",    0x068, "0=none, 1=8ball, 2=all shots" },
        { "Ruleset", "field_0x70",             "void*",  0x070, "" },
        { "Ruleset", "field_0x78",             "void*",  0x078, "" },
        { "Ruleset", "field_0x80",             "int",    0x080, "" },
        { "Ruleset", "field_0x88",             "void*",  0x088, "" },
        { "Ruleset", "field_0x90",             "int",    0x090, "" },
        { "Ruleset", "field_0x98",             "vector", 0x098, "vector<BallClassification>" },
        { "Ruleset", "field_0xB0",             "vector", 0x0B0, "vector<BallClassification>" },
        { "Ruleset", "classificationVector",   "vector", 0x0C8, "player ball classification" },
        { "Ruleset", "field_0xE0",             "vector", 0x0E0, "vector<PoolPlayer*>" },
        { "Ruleset", "field_0xF8",             "int",    0x0F8, "" },
        { "Ruleset", "field_0xFC",             "int",    0x0FC, "" },
        { "Ruleset", "field_0x100",            "int",    0x100, "" },
        { "Ruleset", "field_0x104",            "int",    0x104, "" },
        { "Ruleset", "field_0x108",            "bool",   0x108, "" },
        { "Ruleset", "field_0x10C",            "int",    0x10C, "" },
        { "Ruleset", "field_0x110",            "char",   0x110, "" },
        { "Ruleset", "field_0x111",            "char",   0x111, "" },
        { "Ruleset", "field_0x112",            "bool",   0x112, "" },
        { "Ruleset", "field_0x113",            "bool",   0x113, "" },
        { "Ruleset", "field_0x114",            "bool",   0x114, "" },
        { "Ruleset", "nominatedPocket",        "uint",   0x118, "pocket nomination index (0-5)" },
        { "Ruleset", "field_0x11C",            "int",    0x11C, "" },
        { "Ruleset", "field_0x120",            "char",   0x120, "" },
        { "Ruleset", "field_0x124",            "int",    0x124, "" },
        { "Ruleset", "field_0x128",            "bool",   0x128, "" },
        { "Ruleset", "field_0x12C",            "int",    0x12C, "" },

        // ── Ball ──────────────────────────────────────────────────────────────
        { "Ball", "position",       "Vec2d",  0x020, "world position (x, y)" },
        { "Ball", "velocity",       "Vec2d",  0x030, "velocity vector" },
        { "Ball", "radius",         "double", 0x040, "ball radius" },
        { "Ball", "spin",           "Vec3d",  0x048, "spin vector" },
        { "Ball", "mass",           "double", 0x060, "ball mass" },
        { "Ball", "volume",         "double", 0x068, "ball volume" },
        { "Ball", "classification", "int",    0x0A0, "0=cue, 1=solid, 2=stripe, 3=9ball, 4=8ball" },
        { "Ball", "state",          "int",    0x0A4, "1=table, 2=pocket, 3=unknown, 4=potted" },

        // ── Table ─────────────────────────────────────────────────────────────
        { "Table", "mTableProperties",      "TableProperties",   0x3B0, "" },
        { "Table", "_frictionProperties",   "FrictionProperties",0x3C0, "" },
        { "Table", "mBalls",                "PNSArray<Ball>*",   0x450, "all balls on table" },
        { "Table", "mTableCollisionBounds", "Vec4d",             0x588, "x, y, width, height" },

        // ── TableProperties ───────────────────────────────────────────────────
        { "TableProperties", "mPockets",     "Vec2d*", 0x068, "array of 6 pocket positions" },
        { "TableProperties", "pocketRadius", "double", 0x000, "8.0 (constant)" },
        { "TableProperties", "tableLength",  "double", 0x000, "254.0 (constant)" },
        { "TableProperties", "tableWidth",   "double", 0x000, "127.0 (constant)" },

        // ── VisualCue ─────────────────────────────────────────────────────────
        { "VisualCue", "mVisualGuide", "VisualGuide", 0x3A8, "" },
        { "VisualCue", "mPower",       "double",      0x3B0, "shot power 0.0-1.0" },

        // ── VisualGuide ───────────────────────────────────────────────────────
        { "VisualGuide", "mAimAngle",      "double", 0x028, "shot angle in radians" },
        { "VisualGuide", "mClassification","ptr",    0x0A0, "isVisualGuidePointingToWrongBallClassification" },

        // ── VisualEnglishControl ──────────────────────────────────────────────
        { "VisualEnglishControl", "mEnglish",   "Vec2d",  0x000, "english / spin input" },
        { "VisualEnglishControl", "mAimOffset", "Vec2d",  0x010, "aim offset" },

        // ── StateManager / GameStateManager ───────────────────────────────────
        { "StateManager",     "mStateStack",       "PNSArray<State>*", 0x008, "stack of game states" },
        { "State",            "mStateId",          "int32_t",          0x000, "current state id" },
        { "GameStateManager", "getCurrentStateId", "int32_t",          0x000, "1=menu, 4=my turn, 7=opp turn, 10=ended" },

        // ── Hook Offsets (relative to libmain base) ───────────────────────────
        { "Hook",  "setActiveVisualCue",  "func",  0x2D911E0, "libmain + offset" },
        { "Hook",  "isBallBallCollision", "func",  0x2B1B158, "Prediction physics hook" },
        { "Hook",  "willCollideWithTable","func",  0x3606C80, "Prediction physics hook" },

        // ── Global Pointers (libmain + offset) ────────────────────────────────
        { "GlobalPtr", "sharedDirector",     "ptr", 0x4F06288, "libmain + offset" },
        { "GlobalPtr", "sharedUserInfo",     "ptr", 0x4E9FEB8, "libmain + offset" },
        { "GlobalPtr", "sharedMainManager",  "ptr", 0x4DDE3E0, "libmain + offset" },
        { "GlobalPtr", "sharedMenuManager",  "ptr", 0x4DFE838, "libmain + offset" },
    };

    static constexpr int DB_SIZE = (int)(sizeof(s_database) / sizeof(s_database[0]));

    // ── UI State ──────────────────────────────────────────────────────────────
    static char  s_searchBuf[128]       = {};
    static char  s_lastQuery[128]       = { '\1' }; // force first refresh
    static char  s_copyStatus[64]       = {};
    static float s_copyStatusTimer      = 0.0f;

    struct FilteredEntry {
        const Entry* e;
        char         hexOffset[20];
    };
    static std::vector<FilteredEntry> s_results;
    static bool s_showAll = true; // true = no filter applied yet

    // ── Lowercase helper ──────────────────────────────────────────────────────
    static std::string toLower(const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return r;
    }

    // ── Format hex string untuk satu entry ───────────────────────────────────
    static void formatHex(const Entry& e, char* buf, int bufsz) {
        snprintf(buf, bufsz, "0x%X", (unsigned int)e.offset);
    }

    // ── Jalankan filter/search ────────────────────────────────────────────────
    static void RunSearch() {
        s_results.clear();
        std::string q = toLower(std::string(s_searchBuf));
        bool empty = q.empty();

        for (int i = 0; i < DB_SIZE; i++) {
            const Entry& e = s_database[i];
            bool match = empty;
            if (!empty) {
                std::string cls   = toLower(e.className);
                std::string field = toLower(e.fieldName);
                std::string type  = toLower(e.type);
                std::string note  = toLower(e.note);
                // Cek hex offset juga
                char hexbuf[20];
                formatHex(e, hexbuf, sizeof(hexbuf));
                std::string hex   = toLower(hexbuf);
                match = (cls.find(q)   != std::string::npos) ||
                        (field.find(q) != std::string::npos) ||
                        (type.find(q)  != std::string::npos) ||
                        (note.find(q)  != std::string::npos) ||
                        (hex.find(q)   != std::string::npos);
            }
            if (match) {
                FilteredEntry fe;
                fe.e = &e;
                formatHex(e, fe.hexOffset, sizeof(fe.hexOffset));
                s_results.push_back(fe);
            }
        }
        s_showAll = empty;
    }

    // ── Format satu entry ke string teks ─────────────────────────────────────
    static std::string EntryToString(const FilteredEntry& fe) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[%s] %s  %s  %s  %s",
                 fe.e->className, fe.e->fieldName,
                 fe.hexOffset, fe.e->type,
                 fe.e->note[0] ? fe.e->note : "");
        return buf;
    }

    // ── Copy semua hasil ke clipboard ─────────────────────────────────────────
    static std::string BuildAllResultsText() {
        std::ostringstream ss;
        ss << "// OffsetSearch Results (" << s_results.size() << " entries)\n";
        ss << "// Class               Field                    Offset     Type            Note\n";
        for (auto& fe : s_results) {
            char line[256];
            snprintf(line, sizeof(line),
                     "%-20s %-28s %-10s %-16s %s\n",
                     fe.e->className, fe.e->fieldName,
                     fe.hexOffset, fe.e->type,
                     fe.e->note);
            ss << line;
        }
        return ss.str();
    }

    // ── Set status copy ───────────────────────────────────────────────────────
    static void SetCopyStatus(const char* msg) {
        snprintf(s_copyStatus, sizeof(s_copyStatus), "%s", msg);
        s_copyStatusTimer = 2.0f;
    }

} // namespace OffsetSearch
