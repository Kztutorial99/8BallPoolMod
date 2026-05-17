#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <cmath>

// ── Offset Search + Live Validator ────────────────────────────────────────────
// Database semua offset Data Class game 8 Ball Pool.
// - Search real-time by nama class/field/type/offset
// - Live Validator: baca nilai langsung dari memori game yang sedang berjalan
// - Hasil bisa disalin ke clipboard (per baris atau semua sekaligus)
// ─────────────────────────────────────────────────────────────────────────────

namespace OffsetSearch {

    // ── Base pointer category ─────────────────────────────────────────────────
    // Menentukan dari mana offset ini dihitung
    enum BaseType : int {
        BT_NONE       = 0,
        BT_GM         = 1,   // GameManager.instance
        BT_RULESET    = 2,   // *(ptr*)(GM + 0x3E0)
        BT_TABLE      = 3,   // *(ptr*)(GM + 0x3E8)
        BT_TABLE_PROPS= 4,   // *(ptr*)(Table + 0x3B0)
        BT_VISUAL_CUE = 5,   // *(ptr*)(GM + 0x4B8)
        BT_VIS_GUIDE  = 6,   // *(ptr*)(VisualCue + 0x3A8)
        BT_VEC        = 7,   // *(ptr*)(GM + 0x4C8)  VisualEnglishControl
        BT_STATE_MGR  = 8,   // *(ptr*)(GM + 0x508)
        BT_BALL_CUE   = 9,   // Cue ball (classification==0) instance
        BT_LIBMAIN    = 10,  // absolute: libmain + offset
        BT_LIBMAIN_PTR= 11,  // *(ptr*)(libmain + offset) — deref global ptr
    };

    // ── Entry satu baris offset ───────────────────────────────────────────────
    struct Entry {
        const char* className;
        const char* fieldName;
        const char* type;
        uintptr_t   offset;
        const char* note;
        BaseType    base;
    };

    // ── Database semua offset ─────────────────────────────────────────────────
    static const Entry s_database[] = {

        // ── GameManager ───────────────────────────────────────────────────────
        { "GameManager", "_rules",                  "ptr",          0x3E0, "Ruleset*",                         BT_GM },
        { "GameManager", "mTable",                  "ptr",          0x3E8, "Table*",                           BT_GM },
        { "GameManager", "mVisualCue",              "ptr",          0x4B8, "VisualCue*",                       BT_GM },
        { "GameManager", "mVisualEnglishControl",   "ptr",          0x4C8, "VisualEnglishControl*",            BT_GM },
        { "GameManager", "mStateManager",           "ptr",          0x508, "GameStateManager*",                BT_GM },
        { "GameManager", "mGameMode",               "int",          0x5C0, "game mode id",                     BT_GM },

        // ── Ruleset ───────────────────────────────────────────────────────────
        { "Ruleset", "field_0x0",              "int",    0x000, "",                                 BT_RULESET },
        { "Ruleset", "field_0x4",              "int",    0x004, "",                                 BT_RULESET },
        { "Ruleset", "field_0x20",             "int",    0x020, "",                                 BT_RULESET },
        { "Ruleset", "field_0x58",             "bool",   0x058, "",                                 BT_RULESET },
        { "Ruleset", "field_0x60",             "double", 0x060, "",                                 BT_RULESET },
        { "Ruleset", "pocketNominationMode",   "int",    0x068, "0=none, 1=8ball, 2=all shots",     BT_RULESET },
        { "Ruleset", "field_0x80",             "int",    0x080, "",                                 BT_RULESET },
        { "Ruleset", "field_0x90",             "int",    0x090, "",                                 BT_RULESET },
        { "Ruleset", "field_0xF8",             "int",    0x0F8, "",                                 BT_RULESET },
        { "Ruleset", "field_0xFC",             "int",    0x0FC, "",                                 BT_RULESET },
        { "Ruleset", "field_0x100",            "int",    0x100, "",                                 BT_RULESET },
        { "Ruleset", "field_0x104",            "int",    0x104, "",                                 BT_RULESET },
        { "Ruleset", "field_0x108",            "bool",   0x108, "",                                 BT_RULESET },
        { "Ruleset", "field_0x10C",            "int",    0x10C, "",                                 BT_RULESET },
        { "Ruleset", "field_0x110",            "char",   0x110, "",                                 BT_RULESET },
        { "Ruleset", "field_0x111",            "char",   0x111, "",                                 BT_RULESET },
        { "Ruleset", "field_0x112",            "bool",   0x112, "",                                 BT_RULESET },
        { "Ruleset", "field_0x113",            "bool",   0x113, "",                                 BT_RULESET },
        { "Ruleset", "field_0x114",            "bool",   0x114, "",                                 BT_RULESET },
        { "Ruleset", "nominatedPocket",        "uint",   0x118, "pocket nomination index (0-5)",    BT_RULESET },
        { "Ruleset", "field_0x11C",            "int",    0x11C, "",                                 BT_RULESET },
        { "Ruleset", "field_0x124",            "int",    0x124, "",                                 BT_RULESET },
        { "Ruleset", "field_0x128",            "bool",   0x128, "",                                 BT_RULESET },
        { "Ruleset", "field_0x12C",            "int",    0x12C, "",                                 BT_RULESET },

        // ── Ball (cue ball) ───────────────────────────────────────────────────
        { "Ball", "position.x",     "double", 0x020, "world X position",                     BT_BALL_CUE },
        { "Ball", "position.y",     "double", 0x028, "world Y position",                     BT_BALL_CUE },
        { "Ball", "velocity.x",     "double", 0x030, "velocity X",                           BT_BALL_CUE },
        { "Ball", "velocity.y",     "double", 0x038, "velocity Y",                           BT_BALL_CUE },
        { "Ball", "radius",         "double", 0x040, "ball radius (~6.35)",                  BT_BALL_CUE },
        { "Ball", "spin.x",         "double", 0x048, "spin X",                               BT_BALL_CUE },
        { "Ball", "spin.y",         "double", 0x050, "spin Y",                               BT_BALL_CUE },
        { "Ball", "spin.z",         "double", 0x058, "spin Z",                               BT_BALL_CUE },
        { "Ball", "mass",           "double", 0x060, "ball mass",                            BT_BALL_CUE },
        { "Ball", "volume",         "double", 0x068, "ball volume",                          BT_BALL_CUE },
        { "Ball", "classification", "int",    0x0A0, "0=cue,1=solid,2=stripe,3=9ball,4=8b", BT_BALL_CUE },
        { "Ball", "state",          "int",    0x0A4, "1=table,2=pocket,3=unknown,4=potted",  BT_BALL_CUE },

        // ── Table ─────────────────────────────────────────────────────────────
        { "Table", "mTableProperties",      "ptr",   0x3B0, "TableProperties*",              BT_TABLE },
        { "Table", "_frictionProperties",   "ptr",   0x3C0, "FrictionProperties*",           BT_TABLE },
        { "Table", "mBalls",                "ptr",   0x450, "PNSArray<Ball>* (all balls)",   BT_TABLE },
        { "Table", "mCollisionBounds.x",    "double",0x588, "table x bound",                 BT_TABLE },
        { "Table", "mCollisionBounds.y",    "double",0x590, "table y bound",                 BT_TABLE },
        { "Table", "mCollisionBounds.w",    "double",0x598, "table width bound",             BT_TABLE },
        { "Table", "mCollisionBounds.h",    "double",0x5A0, "table height bound",            BT_TABLE },

        // ── TableProperties ───────────────────────────────────────────────────
        { "TableProperties", "mPockets",    "ptr",   0x068, "Vec2d* (6 pocket positions)",   BT_TABLE_PROPS },

        // ── VisualCue ─────────────────────────────────────────────────────────
        { "VisualCue", "mVisualGuide",  "ptr",    0x3A8, "VisualGuide*",                     BT_VISUAL_CUE },
        { "VisualCue", "mPower",        "double", 0x3B0, "shot power 0.0-1.0",               BT_VISUAL_CUE },

        // ── VisualGuide ───────────────────────────────────────────────────────
        { "VisualGuide", "mAimAngle",       "double", 0x028, "shot angle in radians",        BT_VIS_GUIDE },
        { "VisualGuide", "mClassification", "ptr",    0x0A0, "wrong-ball classification ptr",BT_VIS_GUIDE },

        // ── VisualEnglishControl ──────────────────────────────────────────────
        { "VisualEnglishControl", "mEnglish.x",  "double", 0x000, "english spin X",          BT_VEC },
        { "VisualEnglishControl", "mEnglish.y",  "double", 0x008, "english spin Y",          BT_VEC },
        { "VisualEnglishControl", "mAimOffset.x","double", 0x010, "aim offset X",            BT_VEC },
        { "VisualEnglishControl", "mAimOffset.y","double", 0x018, "aim offset Y",            BT_VEC },

        // ── StateManager ──────────────────────────────────────────────────────
        { "StateManager", "mStateStack",    "ptr",     0x008, "PNSArray<State>*",            BT_STATE_MGR },

        // ── Hook Offsets (relative to libmain) ───────────────────────────────
        { "Hook", "setActiveVisualCue",   "func", 0x2D911E0, "libmain + offset",             BT_LIBMAIN },
        { "Hook", "isBallBallCollision",  "func", 0x2B1B158, "Prediction physics",           BT_LIBMAIN },
        { "Hook", "willCollideWithTable", "func", 0x3606C80, "Prediction physics",           BT_LIBMAIN },
        { "Hook", "CUE_SPIN",             "double",0x4E49418,"libmain constant",             BT_LIBMAIN },
        { "Hook", "CUE_MAX_POWER",        "double",0x4E49410,"libmain constant",             BT_LIBMAIN },

        // ── Global Pointers (libmain + offset = address of the ptr) ──────────
        { "GlobalPtr", "sharedDirector",    "ptr", 0x4F06288, "libmain + offset",            BT_LIBMAIN_PTR },
        { "GlobalPtr", "sharedUserInfo",    "ptr", 0x4E9FEB8, "libmain + offset",            BT_LIBMAIN_PTR },
        { "GlobalPtr", "sharedMainManager", "ptr", 0x4DDE3E0, "libmain + offset",            BT_LIBMAIN_PTR },
        { "GlobalPtr", "sharedMenuManager", "ptr", 0x4DFE838, "libmain + offset",            BT_LIBMAIN_PTR },
    };

    static constexpr int DB_SIZE = (int)(sizeof(s_database) / sizeof(s_database[0]));

    // ── UI / Search State ─────────────────────────────────────────────────────
    static char  s_searchBuf[128]  = {};
    static char  s_lastQuery[128]  = { '\1' };
    static char  s_copyStatus[64]  = {};
    static float s_copyStatusTimer = 0.0f;
    static bool  s_validatorOn     = false;  // Live Validator toggle

    struct FilteredEntry {
        const Entry* e;
        char hexOffset[20];
        char liveValue[64];    // nilai live dari memori
        bool liveValid;        // apakah base ptr valid saat ini
    };
    static std::vector<FilteredEntry> s_results;

    // ── Safe memory read ──────────────────────────────────────────────────────
    static inline bool IsAddrSafe(uintptr_t addr) {
        return addr > 0x1000UL && addr < 0x7FFFFFFFFFFFULL;
    }
    static inline uint64_t Rd64(uintptr_t a) {
        return IsAddrSafe(a) ? *(uint64_t*)a : 0ULL;
    }
    static inline uint32_t Rd32(uintptr_t a) {
        return IsAddrSafe(a) ? *(uint32_t*)a : 0U;
    }
    static inline uint8_t Rd8(uintptr_t a) {
        return IsAddrSafe(a) ? *(uint8_t*)a : 0U;
    }
    static inline double RdDbl(uintptr_t a) {
        if (!IsAddrSafe(a)) return 0.0;
        double v = *(double*)a;
        return (std::isnan(v) || std::isinf(v)) ? 0.0 : v;
    }

    // ── Resolve base pointer for each BaseType ────────────────────────────────
    static uintptr_t ResolveBase(BaseType bt) {
        if (!libmain) return 0;

        switch (bt) {
            case BT_GM: {
                if (!sharedGameManager || !sharedGameManager.instance) return 0;
                return sharedGameManager.instance;
            }
            case BT_RULESET: {
                uintptr_t gm = ResolveBase(BT_GM);
                if (!gm) return 0;
                return Rd64(gm + 0x3E0);
            }
            case BT_TABLE: {
                uintptr_t gm = ResolveBase(BT_GM);
                if (!gm) return 0;
                return Rd64(gm + 0x3E8);
            }
            case BT_TABLE_PROPS: {
                uintptr_t tbl = ResolveBase(BT_TABLE);
                if (!tbl) return 0;
                return Rd64(tbl + 0x3B0);
            }
            case BT_VISUAL_CUE: {
                uintptr_t gm = ResolveBase(BT_GM);
                if (!gm) return 0;
                return Rd64(gm + 0x4B8);
            }
            case BT_VIS_GUIDE: {
                uintptr_t vc = ResolveBase(BT_VISUAL_CUE);
                if (!vc) return 0;
                return Rd64(vc + 0x3A8);
            }
            case BT_VEC: {
                uintptr_t gm = ResolveBase(BT_GM);
                if (!gm) return 0;
                return Rd64(gm + 0x4C8);
            }
            case BT_STATE_MGR: {
                uintptr_t gm = ResolveBase(BT_GM);
                if (!gm) return 0;
                return Rd64(gm + 0x508);
            }
            case BT_BALL_CUE: {
                // Ambil cue ball: iterasi mBalls, cari classification == 0
                uintptr_t tbl = ResolveBase(BT_TABLE);
                if (!tbl) return 0;
                uintptr_t ballsArr = Rd64(tbl + 0x450);
                if (!IsAddrSafe(ballsArr)) return 0;
                // PNSArray layout: Class(8), Count(8), Max(8), Data(8)
                uintptr_t count = Rd64(ballsArr + 0x08);
                uintptr_t data  = Rd64(ballsArr + 0x18);
                if (!count || !IsAddrSafe(data)) return 0;
                for (uintptr_t i = 0; i < count && i < 20; i++) {
                    uintptr_t ballPtr = Rd64(data + i * 8);
                    if (!IsAddrSafe(ballPtr)) continue;
                    int32_t cls = (int32_t)Rd32(ballPtr + 0xA0);
                    if (cls == 0) return ballPtr; // CUE_BALL
                }
                return 0;
            }
            case BT_LIBMAIN:
            case BT_LIBMAIN_PTR:
                return libmain;
            default:
                return 0;
        }
    }

    // ── Baca dan format nilai live ────────────────────────────────────────────
    static void ReadLiveValue(FilteredEntry& fe) {
        const Entry& e = *fe.e;
        fe.liveValue[0] = '\0';
        fe.liveValid    = false;

        uintptr_t base = ResolveBase(e.base);

        // GlobalPtr: deref dua kali (base+offset = address of ptr)
        if (e.base == BT_LIBMAIN_PTR) {
            uintptr_t addr = base + e.offset;
            if (!IsAddrSafe(addr)) {
                snprintf(fe.liveValue, sizeof(fe.liveValue), "0x0");
                return;
            }
            uintptr_t val = Rd64(addr);
            snprintf(fe.liveValue, sizeof(fe.liveValue), "0x%lX", (unsigned long)val);
            fe.liveValid = val != 0;
            return;
        }

        // Hook/func offset: tampilkan alamat absolut fungsi
        if (e.base == BT_LIBMAIN) {
            uintptr_t addr = base + e.offset;
            // Untuk double constants, baca sebagai double
            if (strncmp(e.type, "double", 6) == 0) {
                double val = RdDbl(addr);
                snprintf(fe.liveValue, sizeof(fe.liveValue), "%.4f", val);
            } else {
                snprintf(fe.liveValue, sizeof(fe.liveValue), "0x%lX", (unsigned long)addr);
            }
            fe.liveValid = IsAddrSafe(addr);
            return;
        }

        if (!IsAddrSafe(base)) {
            snprintf(fe.liveValue, sizeof(fe.liveValue), "---");
            fe.liveValid = false;
            return;
        }

        uintptr_t addr = base + e.offset;
        fe.liveValid = true;

        const char* t = e.type;

        if (strncmp(t, "double", 6) == 0) {
            double v = RdDbl(addr);
            snprintf(fe.liveValue, sizeof(fe.liveValue), "%.6f", v);
        } else if (strncmp(t, "int", 3) == 0 || strncmp(t, "int32_t", 7) == 0) {
            int32_t v = (int32_t)Rd32(addr);
            snprintf(fe.liveValue, sizeof(fe.liveValue), "%d", v);
        } else if (strncmp(t, "uint", 4) == 0) {
            uint32_t v = Rd32(addr);
            snprintf(fe.liveValue, sizeof(fe.liveValue), "%u", v);
        } else if (strncmp(t, "bool", 4) == 0) {
            uint8_t v = Rd8(addr);
            snprintf(fe.liveValue, sizeof(fe.liveValue), "%s", v ? "true" : "false");
        } else if (strncmp(t, "char", 4) == 0) {
            uint8_t v = Rd8(addr);
            snprintf(fe.liveValue, sizeof(fe.liveValue), "'%c' (0x%02X)", (v >= 0x20 && v < 0x7F) ? v : '.', v);
        } else if (strncmp(t, "ptr", 3) == 0 || strncmp(t, "void*", 5) == 0 || strncmp(t, "func", 4) == 0) {
            uintptr_t v = Rd64(addr);
            if (v && libmain && v >= libmain && v < libmain + 0x10000000)
                snprintf(fe.liveValue, sizeof(fe.liveValue), "lib+0x%lX", (unsigned long)(v - libmain));
            else
                snprintf(fe.liveValue, sizeof(fe.liveValue), "0x%lX", (unsigned long)v);
        } else {
            // Tampilkan raw 8 byte hex
            uint64_t v = Rd64(addr);
            snprintf(fe.liveValue, sizeof(fe.liveValue), "0x%016lX", (unsigned long)v);
        }
    }

    // ── Status base pointers (untuk info panel) ────────────────────────────────
    struct BaseStatus {
        const char* name;
        uintptr_t   addr;
    };

    static void GetBaseStatuses(BaseStatus out[6]) {
        out[0] = { "GameManager",   ResolveBase(BT_GM) };
        out[1] = { "Ruleset",       ResolveBase(BT_RULESET) };
        out[2] = { "Table",         ResolveBase(BT_TABLE) };
        out[3] = { "VisualCue",     ResolveBase(BT_VISUAL_CUE) };
        out[4] = { "StateManager",  ResolveBase(BT_STATE_MGR) };
        out[5] = { "Ball (Cue)",    ResolveBase(BT_BALL_CUE) };
    }

    // ── Lowercase helper ──────────────────────────────────────────────────────
    static std::string toLower(const std::string& s) {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return r;
    }

    static void formatHex(const Entry& e, char* buf, int bufsz) {
        snprintf(buf, bufsz, "0x%X", (unsigned int)e.offset);
    }

    // ── Run search/filter ─────────────────────────────────────────────────────
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
                char hexbuf[20]; formatHex(e, hexbuf, sizeof(hexbuf));
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
                fe.liveValue[0] = '\0';
                fe.liveValid    = false;
                s_results.push_back(fe);
            }
        }
    }

    // ── Update semua nilai live (panggil sekali per frame / per toggle) ───────
    static void RefreshLiveValues() {
        for (auto& fe : s_results) {
            ReadLiveValue(fe);
        }
    }

    // ── Format entry ke string teks untuk clipboard ───────────────────────────
    static std::string EntryToString(const FilteredEntry& fe) {
        char buf[512];
        if (s_validatorOn && fe.liveValue[0]) {
            snprintf(buf, sizeof(buf), "[%s] %-28s %s  %-16s  live=%s  %s",
                     fe.e->className, fe.e->fieldName,
                     fe.hexOffset, fe.e->type,
                     fe.liveValue, fe.e->note);
        } else {
            snprintf(buf, sizeof(buf), "[%s] %-28s %s  %-16s  %s",
                     fe.e->className, fe.e->fieldName,
                     fe.hexOffset, fe.e->type,
                     fe.e->note);
        }
        return buf;
    }

    static std::string BuildAllResultsText() {
        std::ostringstream ss;
        ss << "// OffsetSearch Results (" << s_results.size() << " entries)\n";
        if (s_validatorOn)
            ss << "// Class                Field                    Offset      Type             Live Value       Note\n";
        else
            ss << "// Class                Field                    Offset      Type             Note\n";
        for (auto& fe : s_results) {
            if (s_validatorOn && fe.liveValue[0]) {
                char line[512];
                snprintf(line, sizeof(line),
                         "%-20s %-28s %-10s %-16s %-18s %s\n",
                         fe.e->className, fe.e->fieldName,
                         fe.hexOffset, fe.e->type, fe.liveValue, fe.e->note);
                ss << line;
            } else {
                char line[512];
                snprintf(line, sizeof(line),
                         "%-20s %-28s %-10s %-16s %s\n",
                         fe.e->className, fe.e->fieldName,
                         fe.hexOffset, fe.e->type, fe.e->note);
                ss << line;
            }
        }
        return ss.str();
    }

    static void SetCopyStatus(const char* msg) {
        snprintf(s_copyStatus, sizeof(s_copyStatus), "%s", msg);
        s_copyStatusTimer = 2.0f;
    }

} // namespace OffsetSearch
